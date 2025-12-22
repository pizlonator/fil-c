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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:element-vah265enc
 * @title: vah265enc
 * @short_description: A VA-API based H265 video encoder
 *
 * vah265enc encodes raw video VA surfaces into H.265 bitstreams using
 * the installed and chosen [VA-API](https://01.org/linuxmedia/vaapi)
 * driver.
 *
 * The raw video frames in main memory can be imported into VA surfaces.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc num-buffers=60 ! timeoverlay ! vah265enc ! h265parse ! mp4mux ! filesink location=test.mp4
 * ```
 *
 * Since: 1.22
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvah265enc.h"

#include <gst/codecparsers/gsth265bitwriter.h>
#include <gst/va/gstva.h>
#include <gst/va/gstvavideoformat.h>
#include <gst/video/video.h>
#include <va/va_drmcommon.h>

#include "vacompat.h"
#include "gstvabaseenc.h"
#include "gstvaencoder.h"
#include "gstvacaps.h"
#include "gstvaprofile.h"
#include "gstvadisplay_priv.h"
#include "gstvapluginutils.h"

GST_DEBUG_CATEGORY_STATIC (gst_va_h265enc_debug);
#define GST_CAT_DEFAULT gst_va_h265enc_debug

#define GST_VA_H265_ENC(obj)            ((GstVaH265Enc *) obj)
#define GST_VA_H265_ENC_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_FROM_INSTANCE (obj), GstVaH265EncClass))
#define GST_VA_H265_ENC_CLASS(klass)    ((GstVaH265EncClass *) klass)

typedef struct _GstVaH265Enc GstVaH265Enc;
typedef struct _GstVaH265EncClass GstVaH265EncClass;
typedef struct _GstVaH265EncFrame GstVaH265EncFrame;
typedef struct _GstVaH265LevelLimits GstVaH265LevelLimits;

enum
{
  PROP_KEY_INT_MAX = 1,
  PROP_BFRAMES,
  PROP_IFRAMES,
  PROP_NUM_REF_FRAMES,
  PROP_B_PYRAMID,
  PROP_NUM_SLICES,
  PROP_MIN_QP,
  PROP_MAX_QP,
  PROP_QP_I,
  PROP_QP_P,
  PROP_QP_B,
  PROP_TRELLIS,
  PROP_MBBRC,
  PROP_BITRATE,
  PROP_TARGET_PERCENTAGE,
  PROP_TARGET_USAGE,
  PROP_CPB_SIZE,
  PROP_AUD,
  PROP_NUM_TILE_COLS,
  PROP_NUM_TILE_ROWS,
  PROP_RATE_CONTROL,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

static GstObjectClass *parent_class = NULL;

/* Scale factor for bitrate (HRD bit_rate_scale: min = 6) */
#define SX_BITRATE  6
/* Scale factor for CPB size (HRD cpb_size_scale: min = 4) */
#define SX_CPB_SIZE  4
/* Maximum sizes for common headers (in bits) */
#define MAX_PROFILE_TIER_LEVEL_SIZE 684
#define MAX_VPS_HDR_SIZE 13781
#define MAX_SPS_HDR_SIZE 615
#define MAX_SHORT_TERM_REFPICSET_SIZE 55
#define MAX_VUI_PARAMS_SIZE 267
#define MAX_HRD_PARAMS_SIZE 8196
#define MAX_PPS_HDR_SIZE 274
#define MAX_SLICE_HDR_SIZE 33660

#define MAX_GOP_SIZE  1024

/* The max tiles in column according to spec A1 */
#define MAX_COL_TILES 20
/* The max tiles in row according to spec A1 */
#define MAX_ROW_TILES 22

/* *INDENT-OFF* */
struct _GstVaH265EncClass
{
  GstVaBaseEncClass parent_class;

  GType rate_control_type;
  char rate_control_type_name[64];
  GEnumValue rate_control[16];
};
/* *INDENT-ON* */

struct _GstVaH265Enc
{
  /*< private > */
  GstVaBaseEnc parent;

  /* properties */
  struct
  {
    /* kbps */
    guint bitrate;
    /* VA_RC_XXX */
    guint32 rc_ctrl;
    guint key_int_max;
    guint32 num_ref_frames;
    gboolean b_pyramid;
    guint32 num_bframes;
    guint32 num_iframes;
    guint32 min_qp;
    guint32 max_qp;
    guint32 qp_i;
    guint32 qp_p;
    guint32 qp_b;
    gboolean use_trellis;
    gboolean aud;
    guint32 mbbrc;
    guint32 num_slices;
    guint32 num_tile_cols;
    guint32 num_tile_rows;
    guint32 cpb_size;
    guint32 target_percentage;
    guint32 target_usage;
  } prop;

  /* H265 fields */
  guint32 ctu_size;
  guint32 min_coding_block_size;
  guint32 ctu_width;            /* CTU == Coding Tree Unit */
  guint32 ctu_height;
  /* Aligned to 16 */
  guint32 luma_width;
  guint32 luma_height;
  /* Crop rectangle */
  gboolean conformance_window_flag;
  guint32 conf_win_left_offset;
  guint32 conf_win_right_offset;
  guint32 conf_win_top_offset;
  guint32 conf_win_bottom_offset;

  guint bits_depth_luma_minus8;
  guint bits_depth_chroma_minus8;

  guint8 level_idc;
  /* Set true if high tier */
  gboolean tier_flag;
  const gchar *level_str;
  guint min_cr;

  gboolean aud;
  guint32 packed_headers;

  struct
  {
    guint32 num_slices;
    /* start address in CTUs */
    guint32 *slice_segment_address;
    /* CTUs in this slice */
    guint32 *num_ctu_in_slice;

    gboolean slice_span_tiles;
    guint32 num_tile_cols;
    guint32 num_tile_rows;
    /* CTUs in each tile column */
    guint32 *tile_ctu_cols;
    /* CTUs in each tile row */
    guint32 *tile_ctu_rows;
  } partition;

  struct
  {
    guint8 log2_min_luma_coding_block_size_minus3;
    guint8 log2_diff_max_min_luma_coding_block_size;
    guint8 log2_min_transform_block_size_minus2;
    guint8 log2_diff_max_min_transform_block_size;
    guint8 max_transform_hierarchy_depth_inter;
    guint8 max_transform_hierarchy_depth_intra;

    gboolean separate_colour_plane_flag;
    guint8 colour_plane_id;

    gboolean scaling_list_enabled_flag;
    gboolean scaling_list_data_present_flag;

    gboolean amp_enabled_flag;

    gboolean sample_adaptive_offset_enabled_flag;
    gboolean slice_sao_luma_flag;
    gboolean slice_sao_chroma_flag;

    gboolean pcm_enabled_flag;
    guint8 pcm_sample_bit_depth_luma_minus1;
    guint8 pcm_sample_bit_depth_chroma_minus1;
    guint8 log2_min_pcm_luma_coding_block_size_minus3;
    guint8 log2_max_pcm_luma_coding_block_size_minus3;
    guint8 pcm_loop_filter_disabled_flag;

    gboolean temporal_mvp_enabled_flag;
    gboolean collocated_from_l0_flag;
    guint8 collocated_ref_idx;

    gboolean strong_intra_smoothing_enabled_flag;

    gboolean dependent_slice_segment_flag;

    gboolean sign_data_hiding_enabled_flag;

    gboolean constrained_intra_pred_flag;

    gboolean transform_skip_enabled_flag;

    gboolean cu_qp_delta_enabled_flag;
    uint32_t diff_cu_qp_delta_depth;

    gboolean weighted_pred_flag;
    gboolean weighted_bipred_flag;

    gboolean transquant_bypass_enabled_flag;

    gboolean use_trellis;
  } features;

  struct
  {
    /* frames between two IDR [idr, ...., idr) */
    guint32 idr_period;
    /* How may IDRs we have encoded */
    guint32 total_idr_count;
    /* frames between I/P and P frames [I, B, B, .., B, P) */
    guint32 ip_period;
    /* frames between I frames [I, B, B, .., B, P, ..., I), open GOP */
    guint32 i_period;
    /* B frames between I/P and P. */
    guint32 num_bframes;
    /* Use B pyramid structure in the GOP. */
    gboolean b_pyramid;
    /* Level 0 is the simple B not acting as ref. */
    guint32 highest_pyramid_level;
    /* If open GOP, I frames within a GOP. */
    guint32 num_iframes;
    /* A map of all frames types within a GOP. */
    struct
    {
      guint8 slice_type;
      gboolean is_ref;
      guint8 pyramid_level;
      /* Only for b pyramid */
      gint left_ref_poc_diff;
      gint right_ref_poc_diff;
    } frame_types[MAX_GOP_SIZE];

    /* Max poc within a GOP. */
    guint32 max_pic_order_cnt;
    guint32 log2_max_pic_order_cnt;
    /* current index in the frames types map. */
    guint cur_frame_index;

    /* Total ref frames of forward and backward. */
    guint32 num_ref_frames;
    guint32 max_l0_num;
    guint32 max_l1_num;
    guint32 forward_ref_num;
    guint32 backward_ref_num;
    gboolean low_delay_b_mode;

    guint num_reorder_frames;
    guint max_dpb_size;

    GstVideoCodecFrame *last_keyframe;
  } gop;

  struct
  {
    guint target_usage;
    guint32 rc_ctrl_mode;

    guint32 min_qp;
    guint32 max_qp;
    guint32 qp_i;
    guint32 qp_p;
    guint32 qp_b;
    /* macroblock bitrate control */
    guint32 mbbrc;
    guint target_bitrate;
    guint target_percentage;
    guint max_bitrate;
    /* bitrate (bits) */
    guint max_bitrate_bits;
    guint target_bitrate_bits;
    /* length of CPB buffer */
    guint cpb_size;
    /* length of CPB buffer (bits) */
    guint cpb_length_bits;
  } rc;

  GstH265VPS vps_hdr;
  GstH265SPS sps_hdr;
};

struct _GstVaH265EncFrame
{
  GstVaEncodePicture *picture;
  GstH265SliceType type;
  gboolean is_ref;
  guint pyramid_level;
  /* Only for b pyramid */
  gint left_ref_poc_diff;
  gint right_ref_poc_diff;

  gint poc;
  gboolean last_frame;
};

/**
 * GstVaH265LevelLimits:
 * @level_name: the level name
 * @level_idc: the H.265 level_idc value
 * @MaxLumaPs: the maximum luma picture size
 * @MaxCPBTierMain: the maximum CPB size for Main tier(kbits)
 * @MaxCPBTierHigh: the maximum CPB size for High tier(kbits)
 * @MaxSliceSegPic: the maximum slice segments per picture
 * @MaxTileRows: the maximum number of Tile Rows
 * @MaxTileColumns: the maximum number of Tile Columns
 * @MaxLumaSr: the maximum luma sample rate (samples/sec)
 * @MaxBRTierMain: the maximum video bit rate for Main Tier(kbps)
 * @MaxBRTierHigh: the maximum video bit rate for High Tier(kbps)
 * @MinCr: the mimimum compression ratio
 *
 * The data structure that describes the limits of an H.265 level.
 */
struct _GstVaH265LevelLimits
{
  const gchar *level_name;
  guint8 level_idc;
  guint32 MaxLumaPs;
  guint32 MaxCPBTierMain;
  guint32 MaxCPBTierHigh;
  guint32 MaxSliceSegPic;
  guint32 MaxTileRows;
  guint32 MaxTileColumns;
  guint32 MaxLumaSr;
  guint32 MaxBRTierMain;
  guint32 MaxBRTierHigh;
  guint32 MinCr;
};

/* Table A-1 - Level limits */
/* *INDENT-OFF* */
static const GstVaH265LevelLimits _va_h265_level_limits[] = {
  /* level    idc   MaxLumaPs  MCPBMt  MCPBHt  MSlSeg MTR MTC  MaxLumaSr   MBRMt   MBRHt  MinCr */
  {  "1",     30,    36864,    350,    0,       16,   1,  1,   552960,     128,    0,      2  },
  {  "2",     60,    122880,   1500,   0,       16,   1,  1,   3686400,    1500,   0,      2  },
  {  "2.1",   63,    245760,   3000,   0,       20,   1,  1,   7372800,    3000,   0,      2  },
  {  "3",     90,    552960,   6000,   0,       30,   2,  2,   16588800,   6000,   0,      2  },
  {  "3.1",   93,    983040,   10000,  0,       40,   3,  3,   33177600,   10000,  0,      2  },
  {  "4",     120,   2228224,  12000,  30000,   75,   5,  5,   66846720,   12000,  30000,  4  },
  {  "4.1",   123,   2228224,  20000,  50000,   75,   5,  5,   133693440,  20000,  50000,  4  },
  {  "5",     150,   8912896,  25000,  100000,  200,  11, 10,  267386880,  25000,  100000, 6  },
  {  "5.1",   153,   8912896,  40000,  160000,  200,  11, 10,  534773760,  40000,  160000, 8  },
  {  "5.2",   156,   8912896,  60000,  240000,  200,  11, 10,  1069547520, 60000,  240000, 8  },
  {  "6",     180,   35651584, 60000,  240000,  600,  22, 20,  1069547520, 60000,  240000, 8  },
  {  "6.1",   183,   35651584, 120000, 480000,  600,  22, 20,  2139095040, 120000, 480000, 8  },
  {  "6.2",   186,   35651584, 240000, 800000,  600,  22, 20,  4278190080, 240000, 800000, 6  },
};
/* *INDENT-ON* */

#ifndef GST_DISABLE_GST_DEBUG
static const gchar *
_h265_slice_type_name (GstH265SliceType type)
{
  switch (type) {
    case GST_H265_P_SLICE:
      return "P";
    case GST_H265_B_SLICE:
      return "B";
    case GST_H265_I_SLICE:
      return "I";
    default:
      g_assert_not_reached ();
  }

  return NULL;
}

static const gchar *
_rate_control_get_name (guint32 rc_mode)
{
  GParamSpecEnum *spec;
  guint i;

  if (!(properties[PROP_RATE_CONTROL]
          && G_IS_PARAM_SPEC_ENUM (properties[PROP_RATE_CONTROL])))
    return NULL;

  spec = G_PARAM_SPEC_ENUM (properties[PROP_RATE_CONTROL]);
  for (i = 0; i < spec->enum_class->n_values; i++) {
    if (spec->enum_class->values[i].value == rc_mode)
      return spec->enum_class->values[i].value_nick;
  }

  return NULL;
}
#endif /* end of GST_DISABLE_GST_DEBUG */

static GstVaH265EncFrame *
gst_va_h265_enc_frame_new (void)
{
  GstVaH265EncFrame *frame;

  frame = g_new (GstVaH265EncFrame, 1);
  frame->last_frame = FALSE;
  frame->picture = NULL;

  return frame;
}

static void
gst_va_h265_enc_frame_free (gpointer pframe)
{
  GstVaH265EncFrame *frame = pframe;
  g_clear_pointer (&frame->picture, gst_va_encode_picture_free);
  g_free (frame);
}

static inline GstVaH265EncFrame *
_enc_frame (GstVideoCodecFrame * frame)
{
  GstVaH265EncFrame *enc_frame = gst_video_codec_frame_get_user_data (frame);
  g_assert (enc_frame);
  return enc_frame;
}

static inline gboolean
_is_tile_enabled (GstVaH265Enc * self)
{
  return self->partition.num_tile_cols * self->partition.num_tile_rows > 1;
}

static inline gboolean
_is_scc_enabled (GstVaH265Enc * self)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);

  if (base->profile == VAProfileHEVCSccMain
      || base->profile == VAProfileHEVCSccMain10
      || base->profile == VAProfileHEVCSccMain444
      || base->profile == VAProfileHEVCSccMain444_10)
    return TRUE;

  return FALSE;
}

static GstH265NalUnitType
_h265_nal_unit_type (GstVaH265EncFrame * frame)
{
  GstH265NalUnitType nal_unit_type = -1;

  switch (frame->type) {
    case GST_H265_I_SLICE:
      if (frame->poc == 0) {
        nal_unit_type = GST_H265_NAL_SLICE_IDR_W_RADL;
      } else {
        nal_unit_type = GST_H265_NAL_SLICE_TRAIL_R;
      }
      break;
    case GST_H265_P_SLICE:
      nal_unit_type = GST_H265_NAL_SLICE_TRAIL_R;
      break;
    case GST_H265_B_SLICE:
      if (frame->is_ref) {
        nal_unit_type = GST_H265_NAL_SLICE_TRAIL_R;
      } else {
        nal_unit_type = GST_H265_NAL_SLICE_TRAIL_N;
      }
      break;
    default:
      break;
  }

  g_assert (nal_unit_type >= 0);
  return nal_unit_type;
}

static gboolean
_h265_fill_ptl (GstVaH265Enc * self,
    const VAEncSequenceParameterBufferHEVC * sequence,
    GstH265ProfileTierLevel * ptl)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);

  /* *INDENT-OFF* */
  *ptl = (GstH265ProfileTierLevel) {
    .profile_space = 0,
    .tier_flag = sequence->general_tier_flag,
    .profile_idc = sequence->general_profile_idc,
    .profile_compatibility_flag = { 0, },
    .progressive_source_flag = 1,
    .interlaced_source_flag = 0,
    .non_packed_constraint_flag = 0,
    .frame_only_constraint_flag = 1,

    .level_idc = sequence->general_level_idc,
  };
  /* *INDENT-ON* */

  if (sequence->general_profile_idc == 1        /* Main profile */
      /* In A.3.4, NOTE: When general_profile_compatibility_flag[ 3 ] is equal
         to 1, general_profile_compatibility_flag[ 1 ] and
         general_profile_compatibility_flag[ 2 ] should also be equal to 1. */
      || sequence->general_profile_idc == 3     /* Main Still Picture profile */
      ) {
    ptl->profile_compatibility_flag[1] = 1;
  }

  if (
      /* In A.3.2, NOTE: When general_profile_compatibility_flag[ 1 ] is equal
         to 1, general_profile_compatibility_flag[ 2 ] should also be equal to
         1. */
      sequence->general_profile_idc == 1        /* Main profile */
      || sequence->general_profile_idc == 2     /* Main 10 profile */
      /* In A.3.4, NOTE: When general_profile_compatibility_flag[ 3 ] is equal
         to 1, general_profile_compatibility_flag[ 1 ] and
         general_profile_compatibility_flag[ 2 ] should also be equal to 1. */
      || sequence->general_profile_idc == 3     /* Main Still Picture profile */
      ) {
    ptl->profile_compatibility_flag[2] = 1;
  }

  if (sequence->general_profile_idc == 3)
    ptl->profile_compatibility_flag[3] = 1;

  if (sequence->general_profile_idc == 4)       /* format range extensions profiles */
    ptl->profile_compatibility_flag[4] = 1;

  if (sequence->general_profile_idc == 9)       /* screen content coding profiles */
    ptl->profile_compatibility_flag[9] = 1;

  /* additional indications specified for general_profile_idc from 4~10 */
  if (sequence->general_profile_idc == 4) {
    /* In A.3.5, Format range extensions profiles.
       Just support main444, main444-10 main422-10 main422-12 and main-12
       profile now, may add more profiles when needed. */
    switch (base->profile) {
      case VAProfileHEVCMain444:
        ptl->max_12bit_constraint_flag = 1;
        ptl->max_10bit_constraint_flag = 1;
        ptl->max_8bit_constraint_flag = 1;
        ptl->max_422chroma_constraint_flag = 0;
        ptl->max_420chroma_constraint_flag = 0;
        ptl->max_monochrome_constraint_flag = 0;
        ptl->intra_constraint_flag = 0;
        ptl->one_picture_only_constraint_flag = 0;
        ptl->lower_bit_rate_constraint_flag = 1;
        break;
      case VAProfileHEVCMain444_10:
        ptl->max_12bit_constraint_flag = 1;
        ptl->max_10bit_constraint_flag = 1;
        ptl->max_8bit_constraint_flag = 0;
        ptl->max_422chroma_constraint_flag = 0;
        ptl->max_420chroma_constraint_flag = 0;
        ptl->max_monochrome_constraint_flag = 0;
        ptl->intra_constraint_flag = 0;
        ptl->one_picture_only_constraint_flag = 0;
        ptl->lower_bit_rate_constraint_flag = 1;
        break;
      case VAProfileHEVCMain422_10:
        ptl->max_12bit_constraint_flag = 1;
        ptl->max_10bit_constraint_flag = 1;
        ptl->max_8bit_constraint_flag = 0;
        ptl->max_422chroma_constraint_flag = 1;
        ptl->max_420chroma_constraint_flag = 0;
        ptl->max_monochrome_constraint_flag = 0;
        ptl->intra_constraint_flag = 0;
        ptl->one_picture_only_constraint_flag = 0;
        ptl->lower_bit_rate_constraint_flag = 1;
        break;
      case VAProfileHEVCMain422_12:
        ptl->max_12bit_constraint_flag = 1;
        ptl->max_10bit_constraint_flag = 0;
        ptl->max_8bit_constraint_flag = 0;
        ptl->max_422chroma_constraint_flag = 1;
        ptl->max_420chroma_constraint_flag = 0;
        ptl->max_monochrome_constraint_flag = 0;
        ptl->intra_constraint_flag = 0;
        ptl->one_picture_only_constraint_flag = 0;
        ptl->lower_bit_rate_constraint_flag = 1;
        break;
      case VAProfileHEVCMain12:
        ptl->max_12bit_constraint_flag = 1;
        ptl->max_10bit_constraint_flag = 0;
        ptl->max_8bit_constraint_flag = 0;
        ptl->max_422chroma_constraint_flag = 1;
        ptl->max_420chroma_constraint_flag = 1;
        ptl->max_monochrome_constraint_flag = 0;
        ptl->intra_constraint_flag = 0;
        ptl->one_picture_only_constraint_flag = 0;
        ptl->lower_bit_rate_constraint_flag = 1;
        break;
      default:
        GST_WARNING_OBJECT (self, "do not support the profile: %s of"
            " range extensions.", gst_va_profile_name (base->profile));
        goto error;
    }
  } else if (sequence->general_profile_idc == 9) {
    /* In A.3.7, Screen content coding extensions profiles. */
    switch (base->profile) {
      case VAProfileHEVCSccMain:
        ptl->max_14bit_constraint_flag = 1;
        ptl->max_12bit_constraint_flag = 1;
        ptl->max_10bit_constraint_flag = 1;
        ptl->max_8bit_constraint_flag = 1;
        ptl->max_422chroma_constraint_flag = 1;
        ptl->max_420chroma_constraint_flag = 1;
        ptl->max_monochrome_constraint_flag = 0;
        ptl->intra_constraint_flag = 0;
        ptl->one_picture_only_constraint_flag = 0;
        ptl->lower_bit_rate_constraint_flag = 1;
        break;
      case VAProfileHEVCSccMain10:
        ptl->max_14bit_constraint_flag = 1;
        ptl->max_12bit_constraint_flag = 1;
        ptl->max_10bit_constraint_flag = 1;
        ptl->max_8bit_constraint_flag = 0;
        ptl->max_422chroma_constraint_flag = 1;
        ptl->max_420chroma_constraint_flag = 1;
        ptl->max_monochrome_constraint_flag = 0;
        ptl->intra_constraint_flag = 0;
        ptl->one_picture_only_constraint_flag = 0;
        ptl->lower_bit_rate_constraint_flag = 1;
        break;
      case VAProfileHEVCSccMain444:
        ptl->max_14bit_constraint_flag = 1;
        ptl->max_12bit_constraint_flag = 1;
        ptl->max_10bit_constraint_flag = 1;
        ptl->max_8bit_constraint_flag = 1;
        ptl->max_422chroma_constraint_flag = 0;
        ptl->max_420chroma_constraint_flag = 0;
        ptl->max_monochrome_constraint_flag = 0;
        ptl->intra_constraint_flag = 0;
        ptl->one_picture_only_constraint_flag = 0;
        ptl->lower_bit_rate_constraint_flag = 1;
        break;
      case VAProfileHEVCSccMain444_10:
        ptl->max_14bit_constraint_flag = 1;
        ptl->max_12bit_constraint_flag = 1;
        ptl->max_10bit_constraint_flag = 1;
        ptl->max_8bit_constraint_flag = 0;
        ptl->max_422chroma_constraint_flag = 0;
        ptl->max_420chroma_constraint_flag = 0;
        ptl->max_monochrome_constraint_flag = 0;
        ptl->intra_constraint_flag = 0;
        ptl->one_picture_only_constraint_flag = 0;
        ptl->lower_bit_rate_constraint_flag = 1;
        break;
      default:
        GST_WARNING_OBJECT (self, "do not support the profile: %s of screen"
            " content coding extensions.", gst_va_profile_name (base->profile));
        goto error;
    }
  }

  return TRUE;

error:
  GST_WARNING_OBJECT (self, "Failed to write Profile Tier Level");
  return FALSE;
}

/* By now, the VPS is not really used, we just fill all its fields
   with the same info from the SPS. */
static gboolean
_h265_fill_vps (GstVaH265Enc * self,
    const VAEncSequenceParameterBufferHEVC * seq_param)
{
  guint max_dec_pic_buffering =
      self->gop.num_ref_frames + 1 < self->gop.max_dpb_size ?
      self->gop.num_ref_frames + 1 : self->gop.max_dpb_size;

  /* *INDENT-OFF* */
  self->vps_hdr = (GstH265VPS) {
    .id = 0,
    .base_layer_internal_flag = 1,
    .base_layer_available_flag = 1,
    .max_layers_minus1 = 0,
    .max_sub_layers_minus1 = 0,
    .temporal_id_nesting_flag = 1,

    .sub_layer_ordering_info_present_flag = 0,
    .max_dec_pic_buffering_minus1 = { max_dec_pic_buffering - 1, },
    .max_num_reorder_pics = { self->gop.num_reorder_frames, },
    .max_latency_increase_plus1 = { 0, },
    .max_layer_id = 0,
    .num_layer_sets_minus1 = 0,
    .timing_info_present_flag = 0,
    .vps_extension = 0,
  };
  /* *INDENT-ON* */

  if (!_h265_fill_ptl (self, seq_param, &self->vps_hdr.profile_tier_level))
    return FALSE;

  return TRUE;
}

static gboolean
_h265_fill_sps (GstVaH265Enc * self,
    const VAEncSequenceParameterBufferHEVC * seq_param)
{
  guint max_dec_pic_buffering =
      self->gop.num_ref_frames + 1 < self->gop.max_dpb_size ?
      self->gop.num_ref_frames + 1 : self->gop.max_dpb_size;

  g_assert (self->gop.log2_max_pic_order_cnt >= 4);
  /* *INDENT-OFF* */
  self->sps_hdr = (GstH265SPS) {
    .id = 0,
    .vps = &self->vps_hdr,
    .max_sub_layers_minus1 = 0,
    .temporal_id_nesting_flag = 1,

    .chroma_format_idc = seq_param->seq_fields.bits.chroma_format_idc,
    .separate_colour_plane_flag =
        seq_param->seq_fields.bits.separate_colour_plane_flag,
    .pic_width_in_luma_samples = seq_param->pic_width_in_luma_samples,
    .pic_height_in_luma_samples = seq_param->pic_height_in_luma_samples,
    .conformance_window_flag = self->conformance_window_flag,
    .conf_win_left_offset = self->conf_win_left_offset,
    .conf_win_right_offset = self->conf_win_right_offset,
    .conf_win_top_offset = self->conf_win_top_offset,
    .conf_win_bottom_offset = self->conf_win_bottom_offset,
    .bit_depth_luma_minus8 = seq_param->seq_fields.bits.bit_depth_luma_minus8,
    .bit_depth_chroma_minus8 =
        seq_param->seq_fields.bits.bit_depth_chroma_minus8,
    .log2_max_pic_order_cnt_lsb_minus4 = self->gop.log2_max_pic_order_cnt - 4,
    .sub_layer_ordering_info_present_flag = 0,
    .max_dec_pic_buffering_minus1 = { max_dec_pic_buffering - 1, },
    .max_num_reorder_pics = { self->gop.num_reorder_frames, },
    .max_latency_increase_plus1 = { 0, },
    .log2_min_luma_coding_block_size_minus3 =
        seq_param->log2_min_luma_coding_block_size_minus3,
    .log2_diff_max_min_luma_coding_block_size =
        seq_param->log2_diff_max_min_luma_coding_block_size,
    .log2_min_transform_block_size_minus2 =
        seq_param->log2_min_transform_block_size_minus2,
    .log2_diff_max_min_transform_block_size =
        seq_param->log2_diff_max_min_transform_block_size,
    .max_transform_hierarchy_depth_inter =
        seq_param->max_transform_hierarchy_depth_inter,
    .max_transform_hierarchy_depth_intra =
        seq_param->max_transform_hierarchy_depth_intra,
    .scaling_list_enabled_flag =
        seq_param->seq_fields.bits.scaling_list_enabled_flag,
    .scaling_list_data_present_flag =
        self->features.scaling_list_data_present_flag,
    /* Do not change the scaling list now. */
    /* .scaling_list, */
    .amp_enabled_flag = seq_param->seq_fields.bits.amp_enabled_flag,
    .sample_adaptive_offset_enabled_flag =
        seq_param->seq_fields.bits.sample_adaptive_offset_enabled_flag,
    .pcm_enabled_flag = seq_param->seq_fields.bits.pcm_enabled_flag,
    .pcm_sample_bit_depth_luma_minus1 =
        seq_param->pcm_sample_bit_depth_luma_minus1,
    .pcm_sample_bit_depth_chroma_minus1 =
        seq_param->pcm_sample_bit_depth_chroma_minus1,
    .log2_min_pcm_luma_coding_block_size_minus3 =
        seq_param->log2_min_pcm_luma_coding_block_size_minus3,
    .log2_diff_max_min_pcm_luma_coding_block_size =
        seq_param->log2_max_pcm_luma_coding_block_size_minus3 -
        seq_param->log2_min_pcm_luma_coding_block_size_minus3,
    .pcm_loop_filter_disabled_flag =
        seq_param->seq_fields.bits.pcm_loop_filter_disabled_flag,
    .num_short_term_ref_pic_sets = 0,
    .long_term_ref_pics_present_flag = 0,
    .temporal_mvp_enabled_flag =
        seq_param->seq_fields.bits.sps_temporal_mvp_enabled_flag,
    .strong_intra_smoothing_enabled_flag =
        seq_param->seq_fields.bits.strong_intra_smoothing_enabled_flag,
    .vui_parameters_present_flag =
        seq_param->vui_parameters_present_flag,
    .vui_params = {
      .aspect_ratio_info_present_flag =
          seq_param->vui_fields.bits.aspect_ratio_info_present_flag,
      .aspect_ratio_idc = seq_param->aspect_ratio_idc,
      .sar_width = seq_param->sar_width,
      .sar_height = seq_param->sar_height,
      .overscan_info_present_flag = 0,
      .video_signal_type_present_flag = 0,
      .chroma_loc_info_present_flag = 0,
      .neutral_chroma_indication_flag =
          seq_param->vui_fields.bits.neutral_chroma_indication_flag,
      .field_seq_flag = seq_param->vui_fields.bits.field_seq_flag,
      .frame_field_info_present_flag = 0,
      .default_display_window_flag = 0,
      .timing_info_present_flag =
          seq_param->vui_fields.bits.vui_timing_info_present_flag,
      .num_units_in_tick = seq_param->vui_num_units_in_tick,
      .time_scale = seq_param->vui_time_scale,
      .poc_proportional_to_timing_flag = 0,
      /* TODO: provide HRD. */
      .hrd_parameters_present_flag = 0,
      /*
      .hrd_parameters_present_flag = (seq_param->bits_per_second > 0),
      .hrd_params = {
        .nal_hrd_parameters_present_flag = 1,
        .vcl_hrd_parameters_present_flag = 0,
        .sub_pic_hrd_params_present_flag = 0,
        .bit_rate_scale = (SX_BITRATE - 6),
        .cpb_size_scale = (SX_CPB_SIZE - 4),
        .initial_cpb_removal_delay_length_minus1 = 23,
        .au_cpb_removal_delay_length_minus1 = 23,
        .dpb_output_delay_length_minus1 = 23,
        .fixed_pic_rate_general_flag  = { 0, },
        .fixed_pic_rate_within_cvs_flag = { 0, },
        .low_delay_hrd_flag = { 1, },
        .cpb_cnt_minus1 = { 0, },
        .sublayer_hrd_params = {
          { .bit_rate_value_minus1 = { (seq_param->bits_per_second >> SX_BITRATE) - 1, },
            .cpb_size_value_minus1 = { (hrd_params->buffer_size >> SX_CPB_SIZE) - 1, },
            .cpb_size_du_value_minus1 = { 0, },
            .bit_rate_du_value_minus1 = { 0, },
            .cbr_flag = { self->rc_ctrl == VA_RC_CBR, },
          },
        }
      }, */
      .bitstream_restriction_flag =
          seq_param->vui_fields.bits.bitstream_restriction_flag,
      .tiles_fixed_structure_flag =
          seq_param->vui_fields.bits.tiles_fixed_structure_flag,
      .motion_vectors_over_pic_boundaries_flag =
          seq_param->vui_fields.bits.motion_vectors_over_pic_boundaries_flag,
      .restricted_ref_pic_lists_flag =
          seq_param->vui_fields.bits.restricted_ref_pic_lists_flag,
      .min_spatial_segmentation_idc = seq_param->min_spatial_segmentation_idc,
      .max_bytes_per_pic_denom = seq_param->max_bytes_per_pic_denom,
      .max_bits_per_min_cu_denom = seq_param->max_bits_per_min_cu_denom,
      .log2_max_mv_length_horizontal =
          seq_param->vui_fields.bits.log2_max_mv_length_horizontal,
      .log2_max_mv_length_vertical =
          seq_param->vui_fields.bits.log2_max_mv_length_vertical,
    },
    .sps_extension_flag = _is_scc_enabled (self),
    /* if sps_extension_present_flag */
    .sps_range_extension_flag = 0,
    .sps_multilayer_extension_flag = 0,
    .sps_3d_extension_flag = 0,
    .sps_scc_extension_flag = _is_scc_enabled (self),
    /* if sps_scc_extension_flag */
    .sps_scc_extension_params = {
      .sps_curr_pic_ref_enabled_flag = 1,
      .palette_mode_enabled_flag =
          seq_param->scc_fields.bits.palette_mode_enabled_flag,
      .palette_max_size = 64,
      .delta_palette_max_predictor_size = 32,
      .sps_palette_predictor_initializers_present_flag = 0,
      .sps_num_palette_predictor_initializer_minus1 = 0,
      .sps_palette_predictor_initializer = {{ 0, }},
      .motion_vector_resolution_control_idc = 0,
      .intra_boundary_filtering_disabled_flag = 0,
    },
  };
  /* *INDENT-ON* */

  if (!_h265_fill_ptl (self, seq_param, &self->sps_hdr.profile_tier_level))
    return FALSE;

  return TRUE;
}

static void
_h265_fill_pps (GstVaH265Enc * self,
    VAEncPictureParameterBufferHEVC * pic_param,
    GstH265SPS * sps, GstH265PPS * pps)
{
  /* *INDENT-OFF* */
  *pps = (GstH265PPS) {
    .id = 0,
    .sps = sps,
    .dependent_slice_segments_enabled_flag =
        pic_param->pic_fields.bits.dependent_slice_segments_enabled_flag,
    .output_flag_present_flag = 0,
    .num_extra_slice_header_bits = 0,
    .sign_data_hiding_enabled_flag =
        pic_param->pic_fields.bits.sign_data_hiding_enabled_flag,
    .cabac_init_present_flag = 0,
    .num_ref_idx_l0_default_active_minus1 =
        pic_param->num_ref_idx_l0_default_active_minus1,
    .num_ref_idx_l1_default_active_minus1 =
        pic_param->num_ref_idx_l1_default_active_minus1,
    .init_qp_minus26 = pic_param->pic_init_qp - 26,
    .constrained_intra_pred_flag =
        pic_param->pic_fields.bits.constrained_intra_pred_flag,
    .transform_skip_enabled_flag =
        pic_param->pic_fields.bits.transform_skip_enabled_flag,
    .cu_qp_delta_enabled_flag =
        pic_param->pic_fields.bits.cu_qp_delta_enabled_flag,
    .diff_cu_qp_delta_depth = pic_param->diff_cu_qp_delta_depth,
    .cb_qp_offset = pic_param->pps_cb_qp_offset,
    .cr_qp_offset = pic_param->pps_cr_qp_offset,
    .slice_chroma_qp_offsets_present_flag = 0,
    .weighted_pred_flag = pic_param->pic_fields.bits.weighted_pred_flag,
    .weighted_bipred_flag = pic_param->pic_fields.bits.weighted_bipred_flag,
    .transquant_bypass_enabled_flag =
        pic_param->pic_fields.bits.transquant_bypass_enabled_flag,
    .tiles_enabled_flag = pic_param->pic_fields.bits.tiles_enabled_flag,
    .entropy_coding_sync_enabled_flag =
        pic_param->pic_fields.bits.entropy_coding_sync_enabled_flag,
    .num_tile_columns_minus1 = pic_param->num_tile_columns_minus1,
    .num_tile_rows_minus1 = pic_param->num_tile_rows_minus1,
    /* Only support uniform tile mode now. */
    .uniform_spacing_flag = 1,
    .loop_filter_across_tiles_enabled_flag =
        pic_param->pic_fields.bits.loop_filter_across_tiles_enabled_flag,
    .loop_filter_across_slices_enabled_flag =
        pic_param->pic_fields.bits.pps_loop_filter_across_slices_enabled_flag,
    /* Do not change the default deblocking filter */
    .deblocking_filter_control_present_flag = 0,
    .deblocking_filter_override_enabled_flag = 0,
    .deblocking_filter_disabled_flag = 0,
    /* .beta_offset_div2,
       .tc_offset_div2, */
    .scaling_list_data_present_flag =
        pic_param->pic_fields.bits.scaling_list_data_present_flag,
    /* Do not change the scaling list now. */
    /* .scaling_list, */
    /* Do not change the ref list */
    .lists_modification_present_flag = 0,
    .log2_parallel_merge_level_minus2 =
        pic_param->log2_parallel_merge_level_minus2,
    .slice_segment_header_extension_present_flag = 0,
    .pps_extension_flag = _is_scc_enabled (self),
    /* if pps_extension_flag*/
    .pps_range_extension_flag = 0,
    .pps_multilayer_extension_flag = 0,
    .pps_3d_extension_flag = 0,
    .pps_scc_extension_flag = _is_scc_enabled (self),
    /* if pps_scc_extension_flag*/
    .pps_scc_extension_params = {
      .pps_curr_pic_ref_enabled_flag =
          pic_param->scc_fields.bits.pps_curr_pic_ref_enabled_flag,
      .residual_adaptive_colour_transform_enabled_flag = 0,
      .pps_palette_predictor_initializers_present_flag = 0,
    },
  };
  /* *INDENT-ON* */
}

static gboolean
_h265_fill_slice_header (GstVaH265Enc * self, GstVaH265EncFrame * frame,
    GstH265PPS * pps, VAEncSliceParameterBufferHEVC * slice_param,
    gboolean first_slice_segment_in_pic,
    guint list_forward_num, guint list_backward_num,
    gint negative_pocs[16], guint num_negative_pics,
    gint positive_pocs[16], guint num_positive_pics,
    GstH265SliceHdr * slice_hdr)
{
  gint i;
  gint delta_poc;

  /* *INDENT-OFF* */
  *slice_hdr = (GstH265SliceHdr) {
    .pps = pps,
    .first_slice_segment_in_pic_flag = first_slice_segment_in_pic,
    /* set if IDR. */
    .no_output_of_prior_pics_flag = 0,
    .dependent_slice_segment_flag =
        slice_param->slice_fields.bits.dependent_slice_segment_flag,
    .segment_address = slice_param->slice_segment_address,
    .type = slice_param->slice_type,
    /* pps->output_flag_present_flag is not set now. */
    .pic_output_flag = 0,
    .colour_plane_id = slice_param->slice_fields.bits.colour_plane_id,
    /* Set the reference list fields later
    .pic_order_cnt_lsb,
    .short_term_ref_pic_set_sps_flag,
    .short_term_ref_pic_sets,
    .short_term_ref_pic_set_idx,
    .num_long_term_sps,
    .num_long_term_pics,
    .lt_idx_sps[16],
    .poc_lsb_lt[16],
    .used_by_curr_pic_lt_flag[16],
    .delta_poc_msb_present_flag[16],
    .delta_poc_msb_cycle_lt[16], */
    .temporal_mvp_enabled_flag =
        slice_param->slice_fields.bits.slice_temporal_mvp_enabled_flag,
    .sao_luma_flag =
        slice_param->slice_fields.bits.slice_sao_luma_flag,
    .sao_chroma_flag=
        slice_param->slice_fields.bits.slice_sao_chroma_flag,
    /* Set the ref num later
    .num_ref_idx_active_override_flag,
    .num_ref_idx_l0_active_minus1,
    .num_ref_idx_l1_active_minus1,
    .ref_pic_list_modification, */
    .mvd_l1_zero_flag = slice_param->slice_fields.bits.mvd_l1_zero_flag,
    .cabac_init_flag = slice_param->slice_fields.bits.cabac_init_flag,
    .collocated_from_l0_flag =
        slice_param->slice_fields.bits.collocated_from_l0_flag,
    .collocated_ref_idx = (slice_param->slice_type == GST_H265_I_SLICE ?
        0xFF : self->features.collocated_ref_idx),
    /* not used now. */
    .pred_weight_table = { 0, },
    .five_minus_max_num_merge_cand = 5 - slice_param->max_num_merge_cand,
    .use_integer_mv_flag = 0,
    .qp_delta = slice_param->slice_qp_delta,
    .cb_qp_offset = slice_param->slice_cb_qp_offset,
    .cr_qp_offset = slice_param->slice_cr_qp_offset,
    /* SCC is not enabled. */
    .slice_act_y_qp_offset = 0,
    .slice_act_cb_qp_offset = 0,
    .slice_act_cr_qp_offset = 0,

    .cu_chroma_qp_offset_enabled_flag = 0,
    /* Do not change deblocking filter setting. */
    .deblocking_filter_override_flag = 0,
    .deblocking_filter_disabled_flag = 0,
    /* .beta_offset_div2,
       .tc_offset_div2, */
    .loop_filter_across_slices_enabled_flag =
        slice_param->slice_fields.bits.slice_loop_filter_across_slices_enabled_flag,
    .num_entry_point_offsets = 0,
    /* .offset_len_minus1,
       .entry_point_offset_minus1, */
  };
  /* *INDENT-ON* */

  if (slice_hdr->dependent_slice_segment_flag)
    return TRUE;

  if (slice_param->slice_type == GST_H265_I_SLICE)
    return TRUE;

  slice_hdr->pic_order_cnt_lsb = frame->poc;

  /* Write the ref set explicitly. */
  slice_hdr->short_term_ref_pic_set_sps_flag = 0;
  slice_hdr->short_term_ref_pic_sets.inter_ref_pic_set_prediction_flag = 0;
  slice_hdr->short_term_ref_pic_sets.NumDeltaPocs =
      num_negative_pics + num_positive_pics;

  slice_hdr->short_term_ref_pic_sets.NumNegativePics = num_negative_pics;
  for (i = 0; i < num_negative_pics; i++) {
    delta_poc = negative_pocs[i] - frame->poc;
    g_assert (delta_poc < 0);
    slice_hdr->short_term_ref_pic_sets.DeltaPocS0[i] = delta_poc;

    if (i < list_forward_num) {
      slice_hdr->short_term_ref_pic_sets.UsedByCurrPicS0[i] = 1;
    } else {
      slice_hdr->short_term_ref_pic_sets.UsedByCurrPicS0[i] = 0;
    }
  }

  slice_hdr->short_term_ref_pic_sets.NumPositivePics = num_positive_pics;
  for (i = 0; i < num_positive_pics; i++) {
    delta_poc = positive_pocs[i] - frame->poc;
    g_assert (delta_poc > 0);
    slice_hdr->short_term_ref_pic_sets.DeltaPocS1[i] = delta_poc;

    if (i < list_backward_num) {
      slice_hdr->short_term_ref_pic_sets.UsedByCurrPicS1[i] = 1;
    } else {
      slice_hdr->short_term_ref_pic_sets.UsedByCurrPicS1[i] = 0;
    }
  }

  /* For scc, add the current frame into ref */
  if (_is_scc_enabled (self)) {
    slice_hdr->num_ref_idx_active_override_flag = 1;
  } else {
    slice_hdr->num_ref_idx_active_override_flag =
        slice_param->slice_fields.bits.num_ref_idx_active_override_flag;
  }

  if (slice_hdr->num_ref_idx_active_override_flag) {
    if (_is_scc_enabled (self)) {
      /* For scc, need to add 1 for current picture itself when calculating
         NumRpsCurrTempList0. But slice_param->num_ref_idx_l0_active_minus1
         does not include the current frame, but the stream's
         slice_hdr->num_ref_idx_l0_active_minus1 needs to include. */
      if (frame->type == GST_H265_I_SLICE) {
        g_assert (slice_param->num_ref_idx_l0_active_minus1 == 0);
        slice_hdr->num_ref_idx_l0_active_minus1 = 0;
      } else {
        slice_hdr->num_ref_idx_l0_active_minus1 =
            slice_param->num_ref_idx_l0_active_minus1 + 1;
      }
    } else {
      slice_hdr->num_ref_idx_l0_active_minus1 =
          slice_param->num_ref_idx_l0_active_minus1;
    }

    if (slice_param->slice_type == GST_H265_B_SLICE)
      slice_hdr->num_ref_idx_l1_active_minus1 =
          slice_param->num_ref_idx_l1_active_minus1;
  }

  return TRUE;
}

static gboolean
_h265_add_vps_header (GstVaH265Enc * self, GstVaH265EncFrame * frame)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint size;
#define VPS_SIZE 4 + GST_ROUND_UP_8 (MAX_VPS_HDR_SIZE +  \
      MAX_PROFILE_TIER_LEVEL_SIZE + MAX_HRD_PARAMS_SIZE) / 8
  guint8 packed_vps[VPS_SIZE] = { 0, };
#undef VPS_SIZE

  size = sizeof (packed_vps);
  if (gst_h265_bit_writer_vps (&self->vps_hdr, TRUE, packed_vps, &size)
      != GST_H265_BIT_WRITER_OK) {
    GST_ERROR_OBJECT (self, "Failed to write VPS header.");
    return FALSE;
  }

  /* VPS does not have its own packed header define, just reuse
     VAEncPackedHeaderSequence */
  if (!gst_va_encoder_add_packed_header (base->encoder, frame->picture,
          VAEncPackedHeaderSequence, packed_vps, size * 8, FALSE)) {
    GST_ERROR_OBJECT (self, "Failed to add packed VPS header.");
    return FALSE;
  }

  return TRUE;
}

static gboolean
_h265_add_sps_header (GstVaH265Enc * self, GstVaH265EncFrame * frame)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint size;
#define SPS_SIZE 4 + GST_ROUND_UP_8 (MAX_SPS_HDR_SIZE +                  \
      MAX_PROFILE_TIER_LEVEL_SIZE + 64 * MAX_SHORT_TERM_REFPICSET_SIZE + \
      MAX_VUI_PARAMS_SIZE + MAX_HRD_PARAMS_SIZE) / 8
  guint8 packed_sps[SPS_SIZE] = { 0, };
#undef SPS_SIZE

  size = sizeof (packed_sps);
  if (gst_h265_bit_writer_sps (&self->sps_hdr, TRUE, packed_sps, &size)
      != GST_H265_BIT_WRITER_OK) {
    GST_ERROR_OBJECT (self, "Failed to write SPS header.");
    return FALSE;
  }

  if (!gst_va_encoder_add_packed_header (base->encoder, frame->picture,
          VAEncPackedHeaderSequence, packed_sps, size * 8, FALSE)) {
    GST_ERROR_OBJECT (self, "Failed to add packed SPS header.");
    return FALSE;
  }

  return TRUE;
}

static gboolean
_h265_add_pps_header (GstVaH265Enc * self, GstVaH265EncFrame * frame,
    GstH265PPS * pps)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint size;
#define PPS_SIZE 4 + GST_ROUND_UP_8 (MAX_PPS_HDR_SIZE) / 8
  guint8 packed_pps[PPS_SIZE] = { 0, };
#undef PPS_SIZE

  size = sizeof (packed_pps);
  if (gst_h265_bit_writer_pps (pps, TRUE, packed_pps,
          &size) != GST_H265_BIT_WRITER_OK) {
    GST_ERROR_OBJECT (self, "Failed to generate the picture header");
    return FALSE;
  }

  if (!gst_va_encoder_add_packed_header (base->encoder, frame->picture,
          VAEncPackedHeaderPicture, packed_pps, size * 8, FALSE)) {
    GST_ERROR_OBJECT (self, "Failed to add the packed picture header");
    return FALSE;
  }

  return TRUE;
}

static gboolean
_h265_add_slice_header (GstVaH265Enc * self, GstVaH265EncFrame * frame,
    GstH265SliceHdr * slice_hdr)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  GstH265NalUnitType nal_type = _h265_nal_unit_type (frame);
  guint size;
#define SLICE_HDR_SIZE 4 + GST_ROUND_UP_8 (MAX_SLICE_HDR_SIZE) / 8
  guint8 packed_slice_hdr[SLICE_HDR_SIZE] = { 0, };
#undef SLICE_HDR_SIZE

  size = sizeof (packed_slice_hdr);
  if (gst_h265_bit_writer_slice_hdr (slice_hdr, TRUE, nal_type,
          packed_slice_hdr, &size) != GST_H265_BIT_WRITER_OK) {
    GST_ERROR_OBJECT (self, "Failed to generate the slice header");
    return FALSE;
  }

  if (!gst_va_encoder_add_packed_header (base->encoder, frame->picture,
          VAEncPackedHeaderSlice, packed_slice_hdr, size * 8, FALSE)) {
    GST_ERROR_OBJECT (self, "Failed to add the packed slice header");
    return FALSE;
  }

  return TRUE;
}

static gboolean
_h265_add_aud (GstVaH265Enc * self, GstVaH265EncFrame * frame)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint8 aud_data[8] = { 0, };
  guint size;
  guint8 pic_type = 0;

  switch (frame->type) {
    case GST_H265_I_SLICE:
      pic_type = 0;
      break;
    case GST_H265_P_SLICE:
      pic_type = 1;
      break;
    case GST_H265_B_SLICE:
      pic_type = 2;
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  size = sizeof (aud_data);
  if (gst_h265_bit_writer_aud (pic_type, TRUE, aud_data,
          &size) != GST_H265_BIT_WRITER_OK) {
    GST_ERROR_OBJECT (self, "Failed to generate the AUD");
    return FALSE;
  }

  if (!gst_va_encoder_add_packed_header (base->encoder, frame->picture,
          VAEncPackedHeaderRawData, aud_data, size * 8, FALSE)) {
    GST_ERROR_OBJECT (self, "Failed to add the AUD");
    return FALSE;
  }

  return TRUE;
}

/* Returns H.265 chroma_format_idc value from chroma type */
static guint
_h265_get_chroma_format_idc (guint chroma_type)
{
  guint chroma_format_idc;

  switch (chroma_type) {
    case VA_RT_FORMAT_YUV400:
      chroma_format_idc = 0;
      break;
    case VA_RT_FORMAT_YUV420:
    case VA_RT_FORMAT_YUV420_10:
    case VA_RT_FORMAT_YUV420_12:
      chroma_format_idc = 1;
      break;
    case VA_RT_FORMAT_YUV422:
    case VA_RT_FORMAT_YUV422_10:
    case VA_RT_FORMAT_YUV422_12:
      chroma_format_idc = 2;
      break;
    case VA_RT_FORMAT_YUV444:
    case VA_RT_FORMAT_YUV444_10:
    case VA_RT_FORMAT_YUV444_12:
      chroma_format_idc = 3;
      break;
    default:
      GST_DEBUG ("unsupported GstVaapiChromaType value");
      chroma_format_idc = 1;
      break;
  }
  return chroma_format_idc;
}

static gboolean
_h265_fill_sequence_parameter (GstVaH265Enc * self,
    VAEncSequenceParameterBufferHEVC * sequence)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint profile_idc = 0;

  switch (base->profile) {
    case VAProfileHEVCMain:
      profile_idc = GST_H265_PROFILE_IDC_MAIN;
      break;
    case VAProfileHEVCMain10:
      profile_idc = GST_H265_PROFILE_IDC_MAIN;
      break;
    case VAProfileHEVCMain12:
    case VAProfileHEVCMain422_10:
    case VAProfileHEVCMain422_12:
    case VAProfileHEVCMain444:
    case VAProfileHEVCMain444_10:
    case VAProfileHEVCMain444_12:
      profile_idc = GST_H265_PROFILE_IDC_FORMAT_RANGE_EXTENSION;
      break;
    case VAProfileHEVCSccMain:
    case VAProfileHEVCSccMain10:
    case VAProfileHEVCSccMain444:
    case VAProfileHEVCSccMain444_10:
      profile_idc = GST_H265_PROFILE_IDC_SCREEN_CONTENT_CODING;
      break;
    default:
      GST_ERROR_OBJECT (self, "unsupported profile %d", base->profile);
      return FALSE;
  }

  /* *INDENT-OFF* */
  *sequence = (VAEncSequenceParameterBufferHEVC) {
    .general_profile_idc = profile_idc,
    .general_level_idc = self->level_idc,
    .general_tier_flag = self->tier_flag,
    .intra_period =
        self->gop.i_period > 0 ? self->gop.i_period : self->gop.idr_period,
    .intra_idr_period = self->gop.idr_period,
    .ip_period = self->gop.ip_period,
    .bits_per_second = self->rc.target_bitrate_bits,
    .pic_width_in_luma_samples = self->luma_width,
    .pic_height_in_luma_samples = self->luma_height,
    .seq_fields.bits = {
      .chroma_format_idc = _h265_get_chroma_format_idc (base->rt_format),
      .separate_colour_plane_flag = self->features.separate_colour_plane_flag,
      .bit_depth_luma_minus8 = self->bits_depth_luma_minus8,
      .bit_depth_chroma_minus8 = self->bits_depth_chroma_minus8,
      .scaling_list_enabled_flag = self->features.scaling_list_enabled_flag,
      .strong_intra_smoothing_enabled_flag =
          self->features.strong_intra_smoothing_enabled_flag,
      .amp_enabled_flag = self->features.amp_enabled_flag,
      .sample_adaptive_offset_enabled_flag =
          self->features.sample_adaptive_offset_enabled_flag,
      .pcm_enabled_flag = self->features.pcm_enabled_flag,
      .pcm_loop_filter_disabled_flag =
          self->features.pcm_loop_filter_disabled_flag,
      .sps_temporal_mvp_enabled_flag =
          self->features.temporal_mvp_enabled_flag,
      .low_delay_seq = (self->gop.num_bframes == 0),
      .hierachical_flag = self->gop.b_pyramid,
    },
    .log2_min_luma_coding_block_size_minus3 =
        self->features.log2_min_luma_coding_block_size_minus3,
    .log2_diff_max_min_luma_coding_block_size =
        self->features.log2_diff_max_min_luma_coding_block_size,
    .log2_min_transform_block_size_minus2 =
        self->features.log2_min_transform_block_size_minus2,
    .log2_diff_max_min_transform_block_size =
        self->features.log2_diff_max_min_transform_block_size,
    .max_transform_hierarchy_depth_inter =
        self->features.max_transform_hierarchy_depth_inter,
    .max_transform_hierarchy_depth_intra =
        self->features.max_transform_hierarchy_depth_intra,
    /* pcm_enabled_flag is unset, ignore */
    .pcm_sample_bit_depth_luma_minus1 =
        self->features.pcm_sample_bit_depth_luma_minus1,
    .pcm_sample_bit_depth_chroma_minus1 =
        self->features.pcm_sample_bit_depth_chroma_minus1,
    .log2_min_pcm_luma_coding_block_size_minus3 =
        self->features.log2_min_pcm_luma_coding_block_size_minus3,
    .log2_max_pcm_luma_coding_block_size_minus3 =
        self->features.log2_max_pcm_luma_coding_block_size_minus3,
    /* VUI parameters are always set, at least for timing_info (framerate) */
    .vui_parameters_present_flag = TRUE,
    .vui_fields.bits = {
      .aspect_ratio_info_present_flag = TRUE,
      .bitstream_restriction_flag = FALSE,
      .vui_timing_info_present_flag = TRUE,
    },
    /* if (vui_fields.bits.aspect_ratio_info_present_flag) */
    .aspect_ratio_idc = 0xff,
    .sar_width = GST_VIDEO_INFO_PAR_N (&base->in_info),
    .sar_height = GST_VIDEO_INFO_PAR_D (&base->in_info),
    /* if (vui_fields.bits.vui_timing_info_present_flag) */
    .vui_num_units_in_tick = GST_VIDEO_INFO_FPS_D (&base->in_info),
    .vui_time_scale = GST_VIDEO_INFO_FPS_N (&base->in_info),
    .scc_fields.bits.palette_mode_enabled_flag = _is_scc_enabled (self),
  };
  /* *INDENT-ON* */

  return TRUE;
}

static guint
_h265_to_va_coding_type (GstVaH265Enc * self, GstVaH265EncFrame * frame)
{
  guint coding_type = 0;

  switch (frame->type) {
    case GST_H265_I_SLICE:
      coding_type = 1;
      break;
    case GST_H265_P_SLICE:
      if (self->gop.low_delay_b_mode) {
        /* Convert P into forward ref B */
        coding_type = 3;
      } else {
        coding_type = 2;
      }
      break;
    case GST_H265_B_SLICE:
      /* We use hierarchical_level_plus1, so same for all B frames */
      coding_type = 3;
      break;
    default:
      break;
  }

  g_assert (coding_type > 0);
  return coding_type;
}

static inline gboolean
_h265_fill_picture_parameter (GstVaH265Enc * self, GstVaH265EncFrame * frame,
    VAEncPictureParameterBufferHEVC * pic_param, gint collocated_poc)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint8 num_ref_idx_l0_default_active_minus1 = 0;
  guint8 num_ref_idx_l1_default_active_minus1 = 0;
  guint hierarchical_level_plus1 = 0;
  guint i;

  /* *INDENT-OFF* */
  if (self->gop.b_pyramid) {
    /* I/P is the base hierarchical level 0, L0 level B is 1, and so on. */
    hierarchical_level_plus1 = 1;

    if (frame->type == GST_H265_B_SLICE) {
      hierarchical_level_plus1 += 1;
      hierarchical_level_plus1 += frame->pyramid_level;
    }
  }

  if (frame->type == GST_H265_P_SLICE || frame->type == GST_H265_B_SLICE) {
    num_ref_idx_l0_default_active_minus1 =
        (self->gop.forward_ref_num > 0 ? self->gop.forward_ref_num - 1 : 0);
  }
  if (frame->type == GST_H265_B_SLICE) {
    num_ref_idx_l1_default_active_minus1 =
        (self->gop.backward_ref_num > 0 ? self->gop.backward_ref_num - 1 : 0);
  }

  *pic_param = (VAEncPictureParameterBufferHEVC) {
    .decoded_curr_pic.picture_id =
        gst_va_encode_picture_get_reconstruct_surface (frame->picture),
    .decoded_curr_pic.pic_order_cnt = frame->poc,
    .decoded_curr_pic.flags = 0,

    .coded_buf = frame->picture->coded_buffer,
    .last_picture = frame->last_frame,
    .pic_init_qp = self->rc.qp_i,
    .diff_cu_qp_delta_depth = self->features.diff_cu_qp_delta_depth,
    /* Do not use qp offset in picture. */
    .pps_cb_qp_offset = 0,
    .pps_cr_qp_offset = 0,
    /* TODO: multi tile support */
    .num_tile_columns_minus1 = 0,
    .num_tile_rows_minus1 = 0,
    .log2_parallel_merge_level_minus2 = 0,
    .ctu_max_bitsize_allowed = 0,
    .num_ref_idx_l0_default_active_minus1 = num_ref_idx_l0_default_active_minus1,
    .num_ref_idx_l1_default_active_minus1 = num_ref_idx_l1_default_active_minus1,
    .slice_pic_parameter_set_id = 0,
    .nal_unit_type = _h265_nal_unit_type (frame),
    .pic_fields.bits = {
      .idr_pic_flag = (frame->poc == 0),
      .coding_type = _h265_to_va_coding_type (self, frame),
      .reference_pic_flag = frame->is_ref,
      /* allow slice to set dependent_slice_segment_flag */
      .dependent_slice_segments_enabled_flag =
          self->features.dependent_slice_segment_flag,
      .sign_data_hiding_enabled_flag =
          self->features.sign_data_hiding_enabled_flag,
      .constrained_intra_pred_flag = self->features.constrained_intra_pred_flag,
      .transform_skip_enabled_flag = self->features.transform_skip_enabled_flag,
      .cu_qp_delta_enabled_flag = self->features.cu_qp_delta_enabled_flag,
      .weighted_pred_flag = self->features.weighted_pred_flag,
      .weighted_bipred_flag = self->features.weighted_bipred_flag,
      .transquant_bypass_enabled_flag =
          self->features.transquant_bypass_enabled_flag,
      .tiles_enabled_flag = _is_tile_enabled (self),
      .entropy_coding_sync_enabled_flag = 0,
      /* When we enable multi tiles, enable this. */
      .loop_filter_across_tiles_enabled_flag = _is_tile_enabled (self),
      .pps_loop_filter_across_slices_enabled_flag = 1,
      /* Should not change the scaling list, not used now */
      .scaling_list_data_present_flag =
          self->features.scaling_list_data_present_flag,
      .screen_content_flag = 0,
      /* Depend on weighted_pred_flag and weighted_bipred_flag */
      .enable_gpu_weighted_prediction = 0,
      /* set if IDR. */
      .no_output_of_prior_pics_flag = 0,
    },
    /* We use coding_type here, set this to 0. */
    .hierarchical_level_plus1 = hierarchical_level_plus1,
    .scc_fields.bits.pps_curr_pic_ref_enabled_flag =
        _is_scc_enabled (self),
  };
  /* *INDENT-ON* */

  i = 0;
  if (frame->type != GST_H265_I_SLICE) {
    GstVaH265EncFrame *f;

    if (g_queue_is_empty (&base->ref_list)) {
      GST_ERROR_OBJECT (self, "No reference found for frame type %s",
          _h265_slice_type_name (frame->type));
      return FALSE;
    }

    g_assert (g_queue_get_length (&base->ref_list) <= self->gop.num_ref_frames);

    /* ref frames in queue are already sorted by poc. */
    for (; i < g_queue_get_length (&base->ref_list); i++) {
      f = _enc_frame (g_queue_peek_nth (&base->ref_list, i));

      pic_param->reference_frames[i].picture_id =
          gst_va_encode_picture_get_reconstruct_surface (f->picture);
      pic_param->reference_frames[i].pic_order_cnt = f->poc;
      pic_param->reference_frames[i].flags = 0;
    }

    g_assert (i < 15);
  }
  for (; i < 15; i++) {
    pic_param->reference_frames[i].picture_id = VA_INVALID_SURFACE;
    pic_param->reference_frames[i].flags = VA_PICTURE_HEVC_INVALID;
  }

  /* If mvp enabled, collocated_ref_idx specifies the reference index of
     the collocated picture used for temporal motion vector prediction.
     We should find the according index in reference_frames[] here. */
  if (frame->type != GST_H265_I_SLICE
      && self->features.temporal_mvp_enabled_flag) {
    gint index = -1;

    for (i = 0; i < 15; i++) {
      if (pic_param->reference_frames[i].flags != VA_PICTURE_HEVC_INVALID &&
          pic_param->reference_frames[i].pic_order_cnt == collocated_poc) {
        index = i;
        break;
      }
    }

    g_assert (index >= 0);
    pic_param->collocated_ref_pic_index = index;
  } else {
    pic_param->collocated_ref_pic_index = 0xFF;
  }

  /* Setup tile info */
  if (pic_param->pic_fields.bits.tiles_enabled_flag) {
    /* Always set loop filter across tiles enabled now */
    pic_param->pic_fields.bits.loop_filter_across_tiles_enabled_flag = 1;

    pic_param->num_tile_columns_minus1 = self->partition.num_tile_cols - 1;
    pic_param->num_tile_rows_minus1 = self->partition.num_tile_rows - 1;

    /* The VA row_height_minus1 and column_width_minus1 size is 1 smaller
       than the MAX_COL_TILES and MAX_ROW_TILES, which means the driver
       can deduce the last tile's size based on the picture info. We need
       to take care of the array size here. */
    for (i = 0; i < MIN (self->partition.num_tile_cols, 19); i++)
      pic_param->column_width_minus1[i] = self->partition.tile_ctu_cols[i] - 1;
    for (i = 0; i < MIN (self->partition.num_tile_rows, 21); i++)
      pic_param->row_height_minus1[i] = self->partition.tile_ctu_rows[i] - 1;
  }

  return TRUE;
}

static gboolean
_h265_fill_slice_parameter (GstVaH265Enc * self, GstVaH265EncFrame * frame,
    guint start_address, gint ctu_num, gboolean last_slice_of_pic,
    GstVaH265EncFrame * list0[16], guint list0_num,
    GstVaH265EncFrame * list1[16], guint list1_num,
    VAEncSliceParameterBufferHEVC * slice)
{
  int8_t slice_qp_delta = 0;
  GstH265SliceType frame_type;
  gint i;

  /* *INDENT-OFF* */
  if (self->rc.rc_ctrl_mode == VA_RC_CQP) {
    if (frame->type == GST_H265_P_SLICE) {
      slice_qp_delta = self->rc.qp_p - self->rc.qp_i;
    } else if (frame->type == GST_H265_B_SLICE) {
      slice_qp_delta = (int8_t) (self->rc.qp_b - self->rc.qp_i);
    }
    g_assert (slice_qp_delta <= 51 && slice_qp_delta >= -51);
  }

  frame_type = frame->type;
  /* If low_delay_b_mode, we convert P to low delay b, which has 2
     ref lists and clone L1 from L0. */
  if (self->gop.low_delay_b_mode && frame->type == GST_H265_P_SLICE) {
    g_assert (self->gop.max_l1_num > 0);
    g_assert (list1_num == 0);

    frame_type = GST_H265_B_SLICE;
    list1_num = (list0_num <= self->gop.max_l1_num ?
        list0_num : self->gop.max_l1_num);

    for (i = 0; i < list1_num; i++)
      list1[i] = list0[i];
  }

  /* In scc mode, the I frame can ref to itself and so the L0 reference
     list is enabled. Then we need to change I frame to P frame because
     it uses L0 list. We just leave all reference unchanged and so all
     ref_pic_list0's picture is invalid, the only ref is itself enabled
     by pic_param->scc_fields.bits.pps_curr_pic_ref_enabled_flag. */
  if (_is_scc_enabled (self) && frame->type == GST_H265_I_SLICE) {
    frame_type = GST_H265_P_SLICE;
    g_assert (list0_num == 0);
  }

  *slice = (VAEncSliceParameterBufferHEVC) {
    .slice_segment_address = start_address,
    .num_ctu_in_slice = ctu_num,
    .slice_type = frame_type,
    /* Only one parameter set supported now. */
    .slice_pic_parameter_set_id = 0,
    /* Set the reference list later
    .num_ref_idx_l0_active_minus1,
    .num_ref_idx_l1_active_minus1,
    .ref_pic_list0[15],
    .ref_pic_list1[15], */
    /* weighted_pred_flag or weighted_bipred_idc is not enabled. */
    .luma_log2_weight_denom = 0,
    .delta_chroma_log2_weight_denom = 0,
    .delta_luma_weight_l0 = { 0, },
    .luma_offset_l0 = { 0, },
    .delta_chroma_weight_l0 = {{ 0, }},
    .chroma_offset_l0 = {{ 0, }},
    .delta_luma_weight_l1 = { 0, },
    .luma_offset_l1 = { 0, },
    .delta_chroma_weight_l1 = {{ 0, }},
    .chroma_offset_l1 = {{ 0, }},

    .max_num_merge_cand = 5,
    .slice_qp_delta = slice_qp_delta,
    .slice_cb_qp_offset = 0,
    .slice_cr_qp_offset = 0,
    /* deblocking_filter_control_present_flag not set now. */
    .slice_beta_offset_div2 = 0,
    .slice_tc_offset_div2 = 0,
    .slice_fields.bits = {
      .last_slice_of_pic_flag = last_slice_of_pic,
      .dependent_slice_segment_flag = (start_address == 0 ? 0 :
          self->features.dependent_slice_segment_flag),
      .colour_plane_id = self->features.colour_plane_id,
      .slice_temporal_mvp_enabled_flag =
          self->features.temporal_mvp_enabled_flag,
      .slice_sao_luma_flag = self->features.slice_sao_luma_flag,
      .slice_sao_chroma_flag = self->features.slice_sao_chroma_flag,
      /* Set the reference list later
      .num_ref_idx_active_override_flag, */
      .mvd_l1_zero_flag = 0,
      /* cabac_init_present_flag is not set now. */
      .cabac_init_flag = 0,
      /* deblocking_filter_control_present_flag not set now */
      .slice_deblocking_filter_disabled_flag = 0,
      .slice_loop_filter_across_slices_enabled_flag = 1,
      .collocated_from_l0_flag = (frame_type == GST_H265_I_SLICE ?
          0 : self->features.collocated_from_l0_flag),
    },
    .pred_weight_table_bit_offset = 0,
    .pred_weight_table_bit_length = 0,
  };
  /* *INDENT-ON* */

  if (frame_type == GST_H265_B_SLICE || frame_type == GST_H265_P_SLICE) {
    slice->slice_fields.bits.num_ref_idx_active_override_flag =
        (list0_num > 0 || list1_num > 0);
    slice->num_ref_idx_l0_active_minus1 = list0_num > 0 ? list0_num - 1 : 0;

    if (frame_type == GST_H265_B_SLICE)
      slice->num_ref_idx_l1_active_minus1 = list1_num > 0 ? list1_num - 1 : 0;
  }

  i = 0;
  if (frame_type != GST_H265_I_SLICE) {
    for (; i < list0_num; i++) {
      slice->ref_pic_list0[i].picture_id =
          gst_va_encode_picture_get_reconstruct_surface (list0[i]->picture);
      slice->ref_pic_list0[i].pic_order_cnt = list0[i]->poc;
    }
  }
  for (; i < G_N_ELEMENTS (slice->ref_pic_list0); ++i) {
    slice->ref_pic_list0[i].picture_id = VA_INVALID_SURFACE;
    slice->ref_pic_list0[i].flags = VA_PICTURE_HEVC_INVALID;
  }

  i = 0;
  if (frame_type == GST_H265_B_SLICE) {
    for (; i < list1_num; i++) {
      slice->ref_pic_list1[i].picture_id =
          gst_va_encode_picture_get_reconstruct_surface (list1[i]->picture);
      slice->ref_pic_list1[i].pic_order_cnt = list1[i]->poc;
    }
  }
  for (; i < G_N_ELEMENTS (slice->ref_pic_list1); ++i) {
    slice->ref_pic_list1[i].picture_id = VA_INVALID_SURFACE;
    slice->ref_pic_list1[i].flags = VA_PICTURE_HEVC_INVALID;
  }

  return TRUE;
}

static gboolean
_h265_add_sequence_parameter (GstVaH265Enc * self, GstVaH265EncFrame * frame,
    VAEncSequenceParameterBufferHEVC * sequence)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);

  if (!gst_va_encoder_add_param (base->encoder, frame->picture,
          VAEncSequenceParameterBufferType, sequence, sizeof (*sequence))) {
    GST_ERROR_OBJECT (self, "Failed to create the sequence parameter");
    return FALSE;
  }

  return TRUE;
}

static gboolean
_h265_add_picture_parameter (GstVaH265Enc * self, GstVaH265EncFrame * frame,
    VAEncPictureParameterBufferHEVC * pic_param)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);

  if (!gst_va_encoder_add_param (base->encoder, frame->picture,
          VAEncPictureParameterBufferType, pic_param,
          sizeof (VAEncPictureParameterBufferHEVC))) {
    GST_ERROR_OBJECT (self, "Failed to create the picture parameter");
    return FALSE;
  }

  return TRUE;
}

static gboolean
_h265_add_slice_parameter (GstVaH265Enc * self, GstVaH265EncFrame * frame,
    VAEncSliceParameterBufferHEVC * slice)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);

  if (!gst_va_encoder_add_param (base->encoder, frame->picture,
          VAEncSliceParameterBufferType, slice,
          sizeof (VAEncSliceParameterBufferHEVC))) {
    GST_ERROR_OBJECT (self, "Failed to add the slice parameter");
    return FALSE;
  }

  return TRUE;
}

static gboolean
_h265_add_slices (GstVaH265Enc * self,
    GstVaH265EncFrame * frame, GstH265PPS * pps,
    GstVaH265EncFrame * list_forward[16], guint list_forward_num,
    GstVaH265EncFrame * list_backward[16], guint list_backward_num,
    gint negative_pocs[16], guint num_negative_pics,
    gint positive_pocs[16], guint num_positive_pics)
{
  guint i_slice;
  VAEncSliceParameterBufferHEVC slice;
  GstH265SliceHdr slice_hdr;

  for (i_slice = 0; i_slice < self->partition.num_slices; i_slice++) {
    if (!_h265_fill_slice_parameter (self, frame,
            self->partition.slice_segment_address[i_slice],
            self->partition.num_ctu_in_slice[i_slice],
            (i_slice == self->partition.num_slices - 1), list_forward,
            list_forward_num, list_backward, list_backward_num, &slice))
      return FALSE;

    if (!_h265_add_slice_parameter (self, frame, &slice))
      return FALSE;

    if (self->packed_headers & VA_ENC_PACKED_HEADER_SLICE) {
      if (!_h265_fill_slice_header (self, frame, pps, &slice, i_slice == 0,
              list_forward_num, list_backward_num, negative_pocs,
              num_negative_pics, positive_pocs, num_positive_pics, &slice_hdr))
        return FALSE;

      if (!_h265_add_slice_header (self, frame, &slice_hdr))
        return FALSE;
    }
  }

  return TRUE;
}

static gint
_poc_asc_compare (const GstVaH265EncFrame ** a, const GstVaH265EncFrame ** b)
{
  return (*a)->poc - (*b)->poc;
}

static gint
_poc_des_compare (const GstVaH265EncFrame ** a, const GstVaH265EncFrame ** b)
{
  return (*b)->poc - (*a)->poc;
}

static gboolean
_h265_encode_one_frame (GstVaH265Enc * self, GstVideoCodecFrame * gst_frame)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  VAEncPictureParameterBufferHEVC pic_param;
  GstH265PPS pps;
  GstVaH265EncFrame *frame;
  GstVaH265EncFrame *list_forward[16] = { NULL, };
  guint list_forward_num = 0;
  GstVaH265EncFrame *list_backward[16] = { NULL, };
  guint list_backward_num = 0;
  gint negative_pocs[16] = { 0, };
  guint num_negative_pics = 0;
  gint positive_pocs[16] = { 0, };
  guint num_positive_pics = 0;
  gint collocated_poc = -1;
  gint i;

  g_return_val_if_fail (gst_frame, FALSE);

  frame = _enc_frame (gst_frame);

  if (self->aud && !_h265_add_aud (self, frame))
    return FALSE;

  /* Repeat the VPS/SPS for IDR. */
  if (frame->poc == 0) {
    VAEncSequenceParameterBufferHEVC sequence;

    if (!gst_va_base_enc_add_rate_control_parameter (base, frame->picture,
            self->rc.rc_ctrl_mode, self->rc.max_bitrate_bits,
            self->rc.target_percentage, self->rc.qp_i, self->rc.min_qp,
            self->rc.max_qp, self->rc.mbbrc))
      return FALSE;

    if (!gst_va_base_enc_add_quality_level_parameter (base, frame->picture,
            self->rc.target_usage))
      return FALSE;

    if (!gst_va_base_enc_add_frame_rate_parameter (base, frame->picture))
      return FALSE;

    if (!gst_va_base_enc_add_hrd_parameter (base, frame->picture,
            self->rc.rc_ctrl_mode, self->rc.cpb_length_bits))
      return FALSE;

    if (!gst_va_base_enc_add_trellis_parameter (base, frame->picture,
            self->features.use_trellis))
      return FALSE;

    _h265_fill_sequence_parameter (self, &sequence);
    if (!_h265_add_sequence_parameter (self, frame, &sequence))
      return FALSE;

    if (self->packed_headers & VA_ENC_PACKED_HEADER_SEQUENCE) {
      if (!_h265_fill_vps (self, &sequence))
        return FALSE;

      if (!_h265_fill_sps (self, &sequence))
        return FALSE;

      if (!_h265_add_vps_header (self, frame))
        return FALSE;

      if (!_h265_add_sps_header (self, frame))
        return FALSE;
    }
  }

  /* Non I frame, construct reference list. */
  if (frame->type != GST_H265_I_SLICE) {
    GstVaH265EncFrame *vaf;
    GstVideoCodecFrame *f;

    for (i = g_queue_get_length (&base->ref_list) - 1; i >= 0; i--) {
      f = g_queue_peek_nth (&base->ref_list, i);
      vaf = _enc_frame (f);
      if (vaf->poc > frame->poc)
        continue;

      list_forward[list_forward_num] = vaf;
      list_forward_num++;
    }

    /* reorder to select the most nearest forward frames. */
    g_qsort_with_data (list_forward, list_forward_num, sizeof (gpointer),
        (GCompareDataFunc) _poc_des_compare, NULL);

    num_negative_pics = list_forward_num;
    for (i = 0; i < list_forward_num; i++)
      negative_pocs[i] = list_forward[i]->poc;

    if (list_forward_num > self->gop.forward_ref_num)
      list_forward_num = self->gop.forward_ref_num;

    if (self->features.temporal_mvp_enabled_flag
        && self->features.collocated_from_l0_flag) {
      if (self->features.collocated_ref_idx >= list_forward_num) {
        GST_ERROR_OBJECT (self, "MVP collocated_ref_idx %d is out of L0 range",
            self->features.collocated_ref_idx);
        return FALSE;
      }

      collocated_poc = list_forward[self->features.collocated_ref_idx]->poc;
    }
  }

  if (frame->type == GST_H265_B_SLICE) {
    GstVaH265EncFrame *vaf;
    GstVideoCodecFrame *f;

    for (i = 0; i < g_queue_get_length (&base->ref_list); i++) {
      f = g_queue_peek_nth (&base->ref_list, i);
      vaf = _enc_frame (f);
      if (vaf->poc < frame->poc)
        continue;

      list_backward[list_backward_num] = vaf;
      list_backward_num++;
    }

    /* reorder to select the most nearest backward frames. */
    g_qsort_with_data (list_backward, list_backward_num, sizeof (gpointer),
        (GCompareDataFunc) _poc_asc_compare, NULL);

    num_positive_pics = list_backward_num;
    for (i = 0; i < list_backward_num; i++)
      positive_pocs[i] = list_backward[i]->poc;

    if (list_backward_num > self->gop.backward_ref_num)
      list_backward_num = self->gop.backward_ref_num;

    if (self->features.temporal_mvp_enabled_flag
        && !self->features.collocated_from_l0_flag) {
      if (self->features.collocated_ref_idx >= list_backward_num) {
        GST_ERROR_OBJECT (self, "MVP collocated_ref_idx %d is out of L1 range",
            self->features.collocated_ref_idx);
        return FALSE;
      }

      collocated_poc = list_backward[self->features.collocated_ref_idx]->poc;
    }
  }

  g_assert (list_forward_num + list_backward_num <= self->gop.num_ref_frames);

  if (!_h265_fill_picture_parameter (self, frame, &pic_param, collocated_poc))
    return FALSE;
  if (!_h265_add_picture_parameter (self, frame, &pic_param))
    return FALSE;

  _h265_fill_pps (self, &pic_param, &self->sps_hdr, &pps);

  if ((self->packed_headers & VA_ENC_PACKED_HEADER_PICTURE)
      && frame->type == GST_H265_I_SLICE
      && !_h265_add_pps_header (self, frame, &pps))
    return FALSE;

  if (!_h265_add_slices (self, frame, &pps,
          list_forward, list_forward_num, list_backward, list_backward_num,
          negative_pocs, num_negative_pics, positive_pocs, num_positive_pics))
    return FALSE;

  if (!gst_va_encoder_encode (base->encoder, frame->picture)) {
    GST_ERROR_OBJECT (self, "Encode frame error");
    return FALSE;
  }

  return TRUE;
}

static gboolean
_h265_push_one_frame (GstVaBaseEnc * base, GstVideoCodecFrame * gst_frame,
    gboolean last)
{
  GstVaH265Enc *self = GST_VA_H265_ENC (base);
  GstVaH265EncFrame *frame;
  gboolean add_cached_key_frame = FALSE;

  g_return_val_if_fail (self->gop.cur_frame_index <= self->gop.idr_period,
      FALSE);

  if (gst_frame) {
    frame = _enc_frame (gst_frame);

    /* Force to insert the key frame inside a GOP, just end the current
       GOP and start a new one. */
    if (GST_VIDEO_CODEC_FRAME_IS_FORCE_KEYFRAME (gst_frame) &&
        !(self->gop.cur_frame_index == 0 ||
            self->gop.cur_frame_index == self->gop.idr_period)) {
      GST_DEBUG_OBJECT (base, "system_frame_number: %d is a force key "
          "frame(IDR), begin a new GOP.", gst_frame->system_frame_number);

      frame->poc = 0;
      frame->type = self->gop.frame_types[0].slice_type;
      frame->is_ref = self->gop.frame_types[0].is_ref;
      frame->pyramid_level = self->gop.frame_types[0].pyramid_level;
      frame->left_ref_poc_diff = self->gop.frame_types[0].left_ref_poc_diff;
      frame->right_ref_poc_diff = self->gop.frame_types[0].right_ref_poc_diff;

      /* The previous key frame should be already be poped out. */
      g_assert (self->gop.last_keyframe == NULL);

      /* An empty reorder list, start the new GOP immediately. */
      if (g_queue_is_empty (&base->reorder_list)) {
        self->gop.cur_frame_index = 1;
        g_queue_clear_full (&base->ref_list,
            (GDestroyNotify) gst_video_codec_frame_unref);
        last = FALSE;
      } else {
        /* Cache the key frame and end the current GOP.
           Next time calling this push() without frame, start the new GOP. */
        self->gop.last_keyframe = gst_frame;
        last = TRUE;
      }

      add_cached_key_frame = TRUE;
    } else {
      /* Begin a new GOP, should have a empty reorder_list. */
      if (self->gop.cur_frame_index == self->gop.idr_period) {
        g_assert (g_queue_is_empty (&base->reorder_list));
        self->gop.cur_frame_index = 0;
      }

      frame->poc = self->gop.cur_frame_index;
      g_assert (self->gop.cur_frame_index <= self->gop.max_pic_order_cnt);

      if (self->gop.cur_frame_index == 0) {
        g_assert (frame->poc == 0);
        GST_LOG_OBJECT (self, "system_frame_number: %d, an IDR frame, starts"
            " a new GOP", gst_frame->system_frame_number);

        g_queue_clear_full (&base->ref_list,
            (GDestroyNotify) gst_video_codec_frame_unref);
      }

      frame->type = self->gop.frame_types[self->gop.cur_frame_index].slice_type;
      frame->is_ref = self->gop.frame_types[self->gop.cur_frame_index].is_ref;
      frame->pyramid_level =
          self->gop.frame_types[self->gop.cur_frame_index].pyramid_level;
      frame->left_ref_poc_diff =
          self->gop.frame_types[self->gop.cur_frame_index].left_ref_poc_diff;
      frame->right_ref_poc_diff =
          self->gop.frame_types[self->gop.cur_frame_index].right_ref_poc_diff;

      GST_LOG_OBJECT (self, "Push frame, system_frame_number: %d, poc %d, "
          "frame type %s", gst_frame->system_frame_number, frame->poc,
          _h265_slice_type_name (frame->type));

      self->gop.cur_frame_index++;
      g_queue_push_tail (&base->reorder_list,
          gst_video_codec_frame_ref (gst_frame));
    }
  } else if (self->gop.last_keyframe) {
    g_assert (self->gop.last_keyframe ==
        g_queue_peek_tail (&base->reorder_list));
    if (g_queue_get_length (&base->reorder_list) == 1) {
      /* The last cached key frame begins a new GOP */
      self->gop.cur_frame_index = 1;
      self->gop.last_keyframe = NULL;
      g_queue_clear_full (&base->ref_list,
          (GDestroyNotify) gst_video_codec_frame_unref);
    }
  }

  /* ensure the last one a non-B and end the GOP. */
  if (last && self->gop.cur_frame_index < self->gop.idr_period) {
    GstVideoCodecFrame *last_frame;

    /* Ensure next push will start a new GOP. */
    self->gop.cur_frame_index = self->gop.idr_period;

    if (!g_queue_is_empty (&base->reorder_list)) {
      last_frame = g_queue_peek_tail (&base->reorder_list);
      frame = _enc_frame (last_frame);
      if (frame->type == GST_H265_B_SLICE) {
        frame->type = GST_H265_P_SLICE;
        frame->is_ref = TRUE;
      }
    }
  }

  /* Insert the cached next key frame after ending the current GOP. */
  if (add_cached_key_frame) {
    g_queue_push_tail (&base->reorder_list,
        gst_video_codec_frame_ref (gst_frame));
  }

  return TRUE;
}

struct RefFramesCount
{
  gint poc;
  guint num;
};

static void
_count_backward_ref_num (gpointer data, gpointer user_data)
{
  GstVaH265EncFrame *frame = _enc_frame (data);
  struct RefFramesCount *count = (struct RefFramesCount *) user_data;

  g_assert (frame->poc != count->poc);
  if (frame->poc > count->poc)
    count->num++;
}

static GstVideoCodecFrame *
_h265_pop_pyramid_b_frame (GstVaH265Enc * self, guint gop_len)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint i;
  gint index = -1;
  GstVaH265EncFrame *b_vaframe;
  GstVideoCodecFrame *b_frame;
  struct RefFramesCount count;

  g_assert (self->gop.backward_ref_num <= 2);

  b_frame = NULL;
  b_vaframe = NULL;

  /* Find the highest level with smallest poc. */
  for (i = 0; i < gop_len; i++) {
    GstVaH265EncFrame *vaf;
    GstVideoCodecFrame *f;

    f = g_queue_peek_nth (&base->reorder_list, i);

    if (!b_frame) {
      b_frame = f;
      b_vaframe = _enc_frame (b_frame);
      index = i;
      continue;
    }

    vaf = _enc_frame (f);
    if (b_vaframe->pyramid_level < vaf->pyramid_level) {
      b_frame = f;
      b_vaframe = vaf;
      index = i;
      continue;
    }

    if (b_vaframe->poc > vaf->poc) {
      b_frame = f;
      b_vaframe = vaf;
      index = i;
    }
  }

again:
  /* Check whether its refs are already poped. */
  g_assert (b_vaframe->left_ref_poc_diff != 0);
  g_assert (b_vaframe->right_ref_poc_diff != 0);
  for (i = 0; i < gop_len; i++) {
    GstVaH265EncFrame *vaf;
    GstVideoCodecFrame *f;

    f = g_queue_peek_nth (&base->reorder_list, i);

    if (f == b_frame)
      continue;

    vaf = _enc_frame (f);
    if (vaf->poc == b_vaframe->poc + b_vaframe->left_ref_poc_diff
        || vaf->poc == b_vaframe->poc + b_vaframe->right_ref_poc_diff) {
      b_frame = f;
      b_vaframe = vaf;
      index = i;
      goto again;
    }
  }

  /* Ensure we already have backward refs */
  count.num = 0;
  count.poc = b_vaframe->poc;
  g_queue_foreach (&base->ref_list, (GFunc) _count_backward_ref_num, &count);
  if (count.num >= 1) {
    GstVideoCodecFrame *f;

    /* it will unref at pop_frame */
    f = g_queue_pop_nth (&base->reorder_list, index);
    g_assert (f == b_frame);
  } else {
    b_frame = NULL;
  }

  return b_frame;
}

static gboolean
_h265_pop_one_frame (GstVaBaseEnc * base, GstVideoCodecFrame ** out_frame)
{
  GstVaH265Enc *self = GST_VA_H265_ENC (base);
  GstVaH265EncFrame *vaframe;
  GstVideoCodecFrame *frame;
  guint gop_len;
  struct RefFramesCount count;

  g_return_val_if_fail (self->gop.cur_frame_index <= self->gop.idr_period,
      FALSE);

  *out_frame = NULL;

  if (g_queue_is_empty (&base->reorder_list))
    return TRUE;

  gop_len = g_queue_get_length (&base->reorder_list);

  if (self->gop.last_keyframe && gop_len > 1)
    gop_len--;

  /* Return the last pushed non-B immediately. */
  frame = g_queue_peek_nth (&base->reorder_list, gop_len - 1);
  vaframe = _enc_frame (frame);
  if (vaframe->type != GST_H265_B_SLICE) {
    frame = g_queue_pop_nth (&base->reorder_list, gop_len - 1);
    goto get_one;
  }

  if (self->gop.b_pyramid) {
    frame = _h265_pop_pyramid_b_frame (self, gop_len);
    if (frame == NULL)
      return TRUE;

    goto get_one;
  }

  g_assert (self->gop.backward_ref_num > 0);

  /* If GOP end, pop anyway. */
  if (self->gop.cur_frame_index == self->gop.idr_period) {
    frame = g_queue_pop_head (&base->reorder_list);
    goto get_one;
  }

  /* Ensure we already have enough backward refs */
  frame = g_queue_peek_head (&base->reorder_list);
  vaframe = _enc_frame (frame);
  count.num = 0;
  count.poc = vaframe->poc;
  g_queue_foreach (&base->ref_list, _count_backward_ref_num, &count);
  if (count.num >= self->gop.backward_ref_num) {
    frame = g_queue_pop_head (&base->reorder_list);
    goto get_one;
  }

  return TRUE;

get_one:
  vaframe = _enc_frame (frame);

  if (vaframe->poc == 0)
    self->gop.total_idr_count++;

  if (self->gop.b_pyramid && vaframe->type == GST_H265_B_SLICE) {
    GST_LOG_OBJECT (self, "pop a pyramid B frame with system_frame_number:"
        " %d, poc: %d, is_ref: %s, level %d",
        frame->system_frame_number, vaframe->poc,
        vaframe->is_ref ? "true" : "false", vaframe->pyramid_level);
  } else {
    GST_LOG_OBJECT (self, "pop a frame with system_frame_number: %d,"
        " frame type: %s, poc: %d, is_ref: %s",
        frame->system_frame_number, _h265_slice_type_name (vaframe->type),
        vaframe->poc, vaframe->is_ref ? "true" : "false");
  }

  /* unref frame popped from queue or pyramid b_frame */
  gst_video_codec_frame_unref (frame);
  *out_frame = frame;
  return TRUE;
}

static gboolean
gst_va_h265_enc_reorder_frame (GstVaBaseEnc * base, GstVideoCodecFrame * frame,
    gboolean bump_all, GstVideoCodecFrame ** out_frame)
{
  if (!_h265_push_one_frame (base, frame, bump_all)) {
    GST_ERROR_OBJECT (base, "Failed to push the input frame"
        " system_frame_number: %d into the reorder list",
        frame->system_frame_number);

    *out_frame = NULL;
    return FALSE;
  }

  if (!_h265_pop_one_frame (base, out_frame)) {
    GST_ERROR_OBJECT (base, "Failed to pop the frame from the reorder list");
    *out_frame = NULL;
    return FALSE;
  }

  return TRUE;
}

static GstVideoCodecFrame *
_h265_find_unused_reference_frame (GstVaH265Enc * self,
    GstVaH265EncFrame * frame)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  GstVaH265EncFrame *b_vaframe;
  GstVideoCodecFrame *b_frame;
  guint i;

  /* We still have more space. */
  if (g_queue_get_length (&base->ref_list) < self->gop.num_ref_frames)
    return NULL;

  /* Not b_pyramid, sliding window is enough. */
  if (!self->gop.b_pyramid)
    return g_queue_peek_head (&base->ref_list);

  /* Non-b ref frame, just pop the first one. */
  if (frame->type != GST_H265_B_SLICE)
    return g_queue_peek_head (&base->ref_list);

  /* Choose the B frame with lowest POC. */
  b_frame = NULL;
  b_vaframe = NULL;
  for (i = 0; i < g_queue_get_length (&base->ref_list); i++) {
    GstVideoCodecFrame *f;
    GstVaH265EncFrame *vaf;

    f = g_queue_peek_nth (&base->ref_list, i);
    vaf = _enc_frame (f);
    if (vaf->type != GST_H265_B_SLICE)
      continue;

    if (!b_frame) {
      g_assert (b_vaframe == NULL);
      b_frame = f;
      b_vaframe = vaf;
      continue;
    }

    g_assert (b_vaframe);
    g_assert (vaf->poc != b_vaframe->poc);
    if (vaf->poc < b_vaframe->poc) {
      b_frame = f;
      b_vaframe = vaf;
    }
  }

  /* No B frame as ref. */
  if (!b_frame)
    return g_queue_peek_head (&base->ref_list);

  if (b_frame != g_queue_peek_head (&base->ref_list)) {
    b_vaframe = _enc_frame (b_frame);
    GST_LOG_OBJECT (self, "The frame with POC: %d will be"
        " replaced by the frame with POC: %d explicitly",
        b_vaframe->poc, frame->poc);
  }

  return b_frame;
}

static gint
_sort_by_poc (gconstpointer a, gconstpointer b, gpointer user_data)
{
  GstVaH265EncFrame *frame1 = _enc_frame ((GstVideoCodecFrame *) a);
  GstVaH265EncFrame *frame2 = _enc_frame ((GstVideoCodecFrame *) b);

  g_assert (frame1->poc != frame2->poc);

  return frame1->poc - frame2->poc;
}

static GstFlowReturn
gst_va_h265_enc_encode_frame (GstVaBaseEnc * base,
    GstVideoCodecFrame * gst_frame, gboolean is_last)
{
  GstVaH265Enc *self = GST_VA_H265_ENC (base);
  GstVaH265EncFrame *frame;
  GstVideoCodecFrame *unused_ref;

  frame = _enc_frame (gst_frame);
  frame->last_frame = is_last;

  g_assert (frame->picture == NULL);
  frame->picture = gst_va_encode_picture_new (base->encoder,
      gst_frame->input_buffer);

  if (!frame->picture) {
    GST_ERROR_OBJECT (base, "Failed to create the encode picture");
    return GST_FLOW_ERROR;
  }

  if (!_h265_encode_one_frame (self, gst_frame)) {
    GST_ERROR_OBJECT (base, "Failed to encode the frame");
    return GST_FLOW_ERROR;
  }

  g_queue_push_tail (&base->output_list, gst_video_codec_frame_ref (gst_frame));

  if (frame->is_ref) {
    unused_ref = _h265_find_unused_reference_frame (self, frame);

    if (unused_ref) {
      if (!g_queue_remove (&base->ref_list, unused_ref))
        g_assert_not_reached ();

      gst_video_codec_frame_unref (unused_ref);
    }

    /* Add it into the reference list. */
    g_queue_push_tail (&base->ref_list, gst_video_codec_frame_ref (gst_frame));
    g_queue_sort (&base->ref_list, _sort_by_poc, NULL);

    g_assert (g_queue_get_length (&base->ref_list) <= self->gop.num_ref_frames);
  }

  return GST_FLOW_OK;
}

/* Clear all the info of last reconfig and set the fields based on
 * property. The reconfig may change these fields because of the
 * profile/level and HW limitation. */
static void
gst_va_h265_enc_reset_state (GstVaBaseEnc * base)
{
  GstVaH265Enc *self = GST_VA_H265_ENC (base);

  GST_VA_BASE_ENC_CLASS (parent_class)->reset_state (base);

  GST_OBJECT_LOCK (self);
  self->features.use_trellis = self->prop.use_trellis;
  self->aud = self->prop.aud;
  self->partition.num_slices = self->prop.num_slices;
  self->partition.num_tile_cols = self->prop.num_tile_cols;
  self->partition.num_tile_rows = self->prop.num_tile_rows;
  self->gop.idr_period = self->prop.key_int_max;
  self->gop.num_bframes = self->prop.num_bframes;
  self->gop.b_pyramid = self->prop.b_pyramid;
  self->gop.num_iframes = self->prop.num_iframes;
  self->gop.num_ref_frames = self->prop.num_ref_frames;
  self->rc.rc_ctrl_mode = self->prop.rc_ctrl;
  self->rc.min_qp = self->prop.min_qp;
  self->rc.max_qp = self->prop.max_qp;
  self->rc.qp_i = self->prop.qp_i;
  self->rc.qp_p = self->prop.qp_p;
  self->rc.qp_b = self->prop.qp_b;
  self->rc.mbbrc = self->prop.mbbrc;
  self->rc.target_percentage = self->prop.target_percentage;
  self->rc.target_usage = self->prop.target_usage;
  self->rc.cpb_size = self->prop.cpb_size;
  GST_OBJECT_UNLOCK (self);

  self->level_idc = 0;
  self->level_str = NULL;
  self->min_cr = 0;
  self->tier_flag = FALSE;
  self->ctu_size = 0;
  self->min_coding_block_size = 0;
  self->ctu_width = 0;
  self->ctu_height = 0;
  self->luma_width = 0;
  self->luma_height = 0;
  self->conformance_window_flag = FALSE;
  self->conf_win_left_offset = 0;
  self->conf_win_right_offset = 0;
  self->conf_win_top_offset = 0;
  self->conf_win_bottom_offset = 0;

  self->bits_depth_luma_minus8 = 0;
  self->bits_depth_chroma_minus8 = 0;

  self->packed_headers = 0;

  self->partition.slice_span_tiles = FALSE;
  g_clear_pointer (&self->partition.slice_segment_address, g_free);
  g_clear_pointer (&self->partition.num_ctu_in_slice, g_free);
  g_clear_pointer (&self->partition.tile_ctu_cols, g_free);
  g_clear_pointer (&self->partition.tile_ctu_rows, g_free);

  self->features.log2_min_luma_coding_block_size_minus3 = 0;
  self->features.log2_diff_max_min_luma_coding_block_size = 0;
  self->features.log2_diff_max_min_luma_coding_block_size = 0;
  self->features.log2_min_transform_block_size_minus2 = 0;
  self->features.log2_diff_max_min_transform_block_size = 0;
  self->features.max_transform_hierarchy_depth_inter = 0;
  self->features.max_transform_hierarchy_depth_intra = 0;
  self->features.separate_colour_plane_flag = FALSE;
  self->features.colour_plane_id = 0;
  self->features.scaling_list_enabled_flag = FALSE;
  self->features.scaling_list_data_present_flag = FALSE;
  self->features.amp_enabled_flag = FALSE;
  self->features.sample_adaptive_offset_enabled_flag = FALSE;
  self->features.slice_sao_luma_flag = FALSE;
  self->features.slice_sao_chroma_flag = FALSE;
  self->features.pcm_enabled_flag = FALSE;
  self->features.pcm_sample_bit_depth_luma_minus1 = 0;
  self->features.pcm_sample_bit_depth_chroma_minus1 = 0;
  self->features.log2_min_pcm_luma_coding_block_size_minus3 = 0;
  self->features.log2_max_pcm_luma_coding_block_size_minus3 = 0;
  self->features.temporal_mvp_enabled_flag = FALSE;
  self->features.collocated_from_l0_flag = FALSE;
  self->features.collocated_ref_idx = 0xFF;
  self->features.strong_intra_smoothing_enabled_flag = FALSE;
  self->features.dependent_slice_segment_flag = FALSE;
  self->features.sign_data_hiding_enabled_flag = FALSE;
  self->features.constrained_intra_pred_flag = FALSE;
  self->features.transform_skip_enabled_flag = FALSE;
  self->features.cu_qp_delta_enabled_flag = FALSE;
  self->features.diff_cu_qp_delta_depth = 0;
  self->features.weighted_pred_flag = FALSE;
  self->features.weighted_bipred_flag = FALSE;
  self->features.transquant_bypass_enabled_flag = FALSE;

  self->gop.i_period = 0;
  self->gop.total_idr_count = 0;
  self->gop.ip_period = 0;
  self->gop.low_delay_b_mode = FALSE;
  self->gop.highest_pyramid_level = 0;
  memset (self->gop.frame_types, 0, sizeof (self->gop.frame_types));
  self->gop.cur_frame_index = 0;
  self->gop.max_pic_order_cnt = 0;
  self->gop.log2_max_pic_order_cnt = 0;
  /* VAEncPictureParameterBufferHEVC.reference_frames limit 15 refs */
  self->gop.max_l0_num = 0;
  self->gop.max_l1_num = 0;
  self->gop.forward_ref_num = 0;
  self->gop.backward_ref_num = 0;
  self->gop.num_reorder_frames = 0;
  self->gop.max_dpb_size = 0;
  self->gop.last_keyframe = NULL;

  self->rc.max_bitrate = 0;
  self->rc.target_bitrate = 0;
  self->rc.max_bitrate_bits = 0;
  self->rc.target_bitrate_bits = 0;
  self->rc.cpb_length_bits = 0;

  memset (&self->vps_hdr, 0, sizeof (GstH265VPS));
  memset (&self->sps_hdr, 0, sizeof (GstH265SPS));
}

static guint
_h265_get_rtformat (GstVaH265Enc * self, GstVideoFormat format,
    guint * depth, guint * chrome)
{
  guint chroma;

  chroma = gst_va_chroma_from_video_format (format);

  switch (chroma) {
    case VA_RT_FORMAT_YUV400:
      *depth = 8;
      *chrome = 0;
      break;
    case VA_RT_FORMAT_YUV420:
      *depth = 8;
      *chrome = 1;
      break;
    case VA_RT_FORMAT_YUV422:
      *depth = 8;
      *chrome = 2;
      break;
    case VA_RT_FORMAT_YUV444:
      *depth = 8;
      *chrome = 3;
      break;
    case VA_RT_FORMAT_YUV420_10:
      *depth = 10;
      *chrome = 1;
      break;
    case VA_RT_FORMAT_YUV422_10:
      *depth = 10;
      *chrome = 2;
      break;
    case VA_RT_FORMAT_YUV444_10:
      *depth = 10;
      *chrome = 3;
      break;
    case VA_RT_FORMAT_YUV420_12:
      *depth = 12;
      *chrome = 1;
      break;
    case VA_RT_FORMAT_YUV422_12:
      *depth = 12;
      *chrome = 2;
      break;
    case VA_RT_FORMAT_YUV444_12:
      *depth = 12;
      *chrome = 3;
      break;
    default:
      chroma = 0;
      GST_ERROR_OBJECT (self, "Unsupported chroma for video format: %s",
          gst_video_format_to_string (format));
      break;
  }

  return chroma;
}

static gboolean
_h265_decide_profile (GstVaH265Enc * self, VAProfile * _profile,
    guint * _rt_format)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  gboolean ret = FALSE;
  GstVideoFormat in_format;
  VAProfile profile;
  guint rt_format;
  GstCaps *allowed_caps = NULL;
  guint num_structures, i, j;
  GstStructure *structure;
  const GValue *v_profile;
  GArray *caps_candidates = NULL;
  GArray *chroma_candidates = NULL;
  guint depth = 0, chrome = 0;

  caps_candidates = g_array_new (TRUE, TRUE, sizeof (VAProfile));
  chroma_candidates = g_array_new (TRUE, TRUE, sizeof (VAProfile));

  /* First, check whether the downstream requires a specified profile. */
  allowed_caps = gst_pad_get_allowed_caps (GST_VIDEO_ENCODER_SRC_PAD (base));
  if (!allowed_caps)
    allowed_caps = gst_pad_query_caps (GST_VIDEO_ENCODER_SRC_PAD (base), NULL);

  if (allowed_caps && !gst_caps_is_empty (allowed_caps)) {
    num_structures = gst_caps_get_size (allowed_caps);
    for (i = 0; i < num_structures; i++) {
      structure = gst_caps_get_structure (allowed_caps, i);
      v_profile = gst_structure_get_value (structure, "profile");
      if (!v_profile)
        continue;

      if (G_VALUE_HOLDS_STRING (v_profile)) {
        profile =
            gst_va_profile_from_name (HEVC, g_value_get_string (v_profile));
        if (profile == VAProfileNone)
          continue;

        g_array_append_val (caps_candidates, profile);
      } else if (GST_VALUE_HOLDS_LIST (v_profile)) {
        guint j;

        for (j = 0; j < gst_value_list_get_size (v_profile); j++) {
          const GValue *p = gst_value_list_get_value (v_profile, j);
          if (!p)
            continue;

          profile = gst_va_profile_from_name (HEVC, g_value_get_string (p));
          if (profile == VAProfileNone)
            continue;
          g_array_append_val (caps_candidates, profile);
        }
      }
    }
  }

  if (caps_candidates->len == 0) {
    GST_ERROR_OBJECT (self, "No available profile in caps");
    ret = FALSE;
    goto out;
  }

  in_format = GST_VIDEO_INFO_FORMAT (&base->in_info);
  rt_format = _h265_get_rtformat (self, in_format, &depth, &chrome);
  if (!rt_format) {
    GST_ERROR_OBJECT (self, "unsupported video format %s",
        gst_video_format_to_string (in_format));
    ret = FALSE;
    goto out;
  }

  /* To make the thing a little simple here, We only consider the bit
     depth compatibility for each level. For example, we will consider
     that Main-4:4:4-10 is able to contain 8 bits 4:4:4 streams, but
     the we wiil not consider that it will contain 10 bits 4:2:0 stream. */
  if (chrome == 3) {
    /* 4:4:4 */
    if (depth == 8) {
      profile = VAProfileHEVCMain444;
      g_array_append_val (chroma_candidates, profile);
      profile = VAProfileHEVCSccMain444;
      g_array_append_val (chroma_candidates, profile);
    }

    if (depth <= 10) {
      profile = VAProfileHEVCMain444_10;
      g_array_append_val (chroma_candidates, profile);
      profile = VAProfileHEVCSccMain444_10;
      g_array_append_val (chroma_candidates, profile);
    }

    if (depth <= 12) {
      profile = VAProfileHEVCMain444_12;
      g_array_append_val (chroma_candidates, profile);
    }
  } else if (chrome == 2) {
    /* 4:2:2 */
    if (depth <= 10) {
      profile = VAProfileHEVCMain422_10;
      g_array_append_val (chroma_candidates, profile);
    }

    if (depth <= 12) {
      profile = VAProfileHEVCMain422_12;
      g_array_append_val (chroma_candidates, profile);
    }
  } else if (chrome == 1 || chrome == 0) {
    /* 4:2:0 or 4:0:0 */
    if (depth == 8) {
      profile = VAProfileHEVCMain;
      g_array_append_val (chroma_candidates, profile);
      profile = VAProfileHEVCSccMain;
      g_array_append_val (chroma_candidates, profile);
    }

    if (depth <= 10) {
      profile = VAProfileHEVCMain10;
      g_array_append_val (chroma_candidates, profile);
      profile = VAProfileHEVCSccMain10;
      g_array_append_val (chroma_candidates, profile);
    }

    if (depth <= 12) {
      profile = VAProfileHEVCMain12;
      g_array_append_val (chroma_candidates, profile);
    }
  }

  /* Just use the first HW available profile in candidate. */
  for (i = 0; i < chroma_candidates->len; i++) {
    profile = g_array_index (chroma_candidates, VAProfile, i);
    if (!gst_va_encoder_has_profile (base->encoder, profile))
      continue;

    if ((rt_format & gst_va_encoder_get_rtformat (base->encoder,
                profile, GST_VA_BASE_ENC_ENTRYPOINT (base))) == 0)
      continue;

    for (j = 0; j < caps_candidates->len; j++) {
      VAProfile p = g_array_index (caps_candidates, VAProfile, j);
      if (profile == p)
        break;
    }
    if (j == caps_candidates->len)
      continue;

    *_profile = profile;
    *_rt_format = rt_format;
    ret = TRUE;
    goto out;
  }

out:
  g_clear_pointer (&caps_candidates, g_array_unref);
  g_clear_pointer (&chroma_candidates, g_array_unref);
  g_clear_pointer (&allowed_caps, gst_caps_unref);

  if (ret) {
    GST_INFO_OBJECT (self, "Select the profile %s",
        gst_va_profile_name (profile));
  } else {
    GST_ERROR_OBJECT (self, "Failed to find an available profile");
  }

  return ret;
}

#define update_property(type, obj, old_val, new_val, prop_id)           \
  gst_va_base_enc_update_property_##type (obj, old_val, new_val, properties[prop_id])
#define update_property_uint(obj, old_val, new_val, prop_id)    \
  update_property (uint, obj, old_val, new_val, prop_id)
#define update_property_bool(obj, old_val, new_val, prop_id)    \
  update_property (bool, obj, old_val, new_val, prop_id)

static void
_h265_calculate_tile_partition (GstVaH265Enc * self)
{
  guint32 ctu_per_slice;
  guint32 left_slices;
  gint32 i, j, k;
  guint32 ctu_tile_width_accu[MAX_COL_TILES + 1];
  guint32 ctu_tile_height_accu[MAX_ROW_TILES + 1];
  /* CTB address in tile scan.
     Add one as sentinel, hold val to calculate ctu_num */
  guint32 *tile_slice_address =
      g_malloc ((self->partition.num_slices + 1) * sizeof (guint32));
  /* map the CTB address in tile scan to CTB raster scan of a picture. */
  guint32 *tile_slice_address_map =
      g_malloc (self->ctu_width * self->ctu_height * sizeof (guint32));

  self->partition.slice_segment_address =
      g_malloc (self->partition.num_slices * sizeof (guint32));
  self->partition.num_ctu_in_slice =
      g_malloc (self->partition.num_slices * sizeof (guint32));
  self->partition.tile_ctu_cols = g_malloc (MAX_COL_TILES * sizeof (guint32));
  self->partition.tile_ctu_rows = g_malloc (MAX_ROW_TILES * sizeof (guint32));

  /* firstly uniformly separate CTUs into tiles, as the spec 6.5.1 define */
  for (i = 0; i < self->partition.num_tile_cols; i++)
    self->partition.tile_ctu_cols[i] =
        ((i + 1) * self->ctu_width) / self->partition.num_tile_cols -
        (i * self->ctu_width) / self->partition.num_tile_cols;
  for (i = 0; i < self->partition.num_tile_rows; i++)
    self->partition.tile_ctu_rows[i] =
        ((i + 1) * self->ctu_height) / self->partition.num_tile_rows -
        (i * self->ctu_height) / self->partition.num_tile_rows;

  /* The requirement that the slice should not span tiles. Firstly we
     should scatter slices uniformly into each tile, bigger tile gets
     more slices. Then we should assign CTUs within one tile uniformly
     to each slice in that tile. */
  if (!self->partition.slice_span_tiles) {
    guint32 *slices_per_tile = g_malloc (self->partition.num_tile_cols *
        self->partition.num_tile_rows * sizeof (guint32));

    ctu_per_slice = (self->ctu_width * self->ctu_height +
        self->partition.num_slices - 1) / self->partition.num_slices;
    g_assert (ctu_per_slice > 0);
    left_slices = self->partition.num_slices;

    for (i = 0;
        i < self->partition.num_tile_cols * self->partition.num_tile_rows;
        i++) {
      slices_per_tile[i] = 1;
      left_slices--;
    }
    while (left_slices) {
      /* Find the biggest CTUs/slices, and assign more. */
      gfloat largest = 0.0f;
      k = -1;
      for (i = 0;
          i < self->partition.num_tile_cols * self->partition.num_tile_rows;
          i++) {
        gfloat f;
        f = ((gfloat)
            (self->partition.tile_ctu_cols[i % self->partition.num_tile_cols] *
                self->partition.tile_ctu_rows
                [i / self->partition.num_tile_cols])) /
            (gfloat) slices_per_tile[i];
        g_assert (f >= 1.0f);
        if (f > largest) {
          k = i;
          largest = f;
        }
      }

      g_assert (k >= 0);
      slices_per_tile[k]++;
      left_slices--;
    }

    /* Assign CTUs in one tile uniformly to each slice. Note: the slice start
       address is CTB address in tile scan(see spec 6.5), that is, we accumulate
       all CTUs in tile0, then tile1, and tile2..., not from the picture's
       perspective. */
    tile_slice_address[0] = 0;
    k = 1;
    for (i = 0; i < self->partition.num_tile_rows; i++) {
      for (j = 0; j < self->partition.num_tile_cols; j++) {
        guint32 s_num = slices_per_tile[i * self->partition.num_tile_cols + j];
        guint32 one_tile_ctus =
            self->partition.tile_ctu_cols[j] * self->partition.tile_ctu_rows[i];
        guint32 s;

        GST_LOG_OBJECT (self, "Tile(row %d col %d), has CTU in col %d,"
            " CTU in row is %d, total CTU %d, assigned %d slices", i, j,
            self->partition.tile_ctu_cols[j], self->partition.tile_ctu_rows[i],
            one_tile_ctus, s_num);

        g_assert (s_num > 0);
        for (s = 0; s < s_num; s++) {
          tile_slice_address[k] = tile_slice_address[k - 1] +
              ((s + 1) * one_tile_ctus) / s_num - (s * one_tile_ctus) / s_num;
          self->partition.num_ctu_in_slice[k - 1] =
              tile_slice_address[k] - tile_slice_address[k - 1];
          k++;
        }
      }
    }

    g_assert (k == self->partition.num_slices + 1);
    /* Calculate the last one */
    self->partition.num_ctu_in_slice[self->partition.num_slices - 1] =
        self->ctu_width * self->ctu_height -
        tile_slice_address[self->partition.num_slices - 1];

    g_free (slices_per_tile);
  }
  /* The easy way, just assign CTUs to each slice uniformly */
  else {
    guint ctu_size, ctu_mod_slice, cur_slice_ctu, last_ctu_index;

    ctu_size = self->ctu_width * self->ctu_height;

    ctu_per_slice = ctu_size / self->partition.num_slices;
    ctu_mod_slice = ctu_size % self->partition.num_slices;
    last_ctu_index = 0;

    for (i = 0; i < self->partition.num_slices; i++) {
      cur_slice_ctu = ctu_per_slice;
      /* Scatter the remainder to each slice */
      if (ctu_mod_slice) {
        ++cur_slice_ctu;
        --ctu_mod_slice;
      }

      tile_slice_address[i] = last_ctu_index;
      self->partition.num_ctu_in_slice[i] = cur_slice_ctu;

      /* set calculation for next slice */
      last_ctu_index += cur_slice_ctu;
      g_assert (last_ctu_index <= ctu_size);
    }
  }

  /* Build the map to specifying the conversion between a CTB address in CTB
     raster scan of a picture and a CTB address in tile scan(see spec 6.5.1
     for details). */
  ctu_tile_width_accu[0] = 0;
  for (i = 1; i <= self->partition.num_tile_cols; i++)
    ctu_tile_width_accu[i] =
        ctu_tile_width_accu[i - 1] + self->partition.tile_ctu_cols[i - 1];

  ctu_tile_height_accu[0] = 0;
  for (i = 1; i <= self->partition.num_tile_rows; i++)
    ctu_tile_height_accu[i] =
        ctu_tile_height_accu[i - 1] + self->partition.tile_ctu_rows[i - 1];

  for (k = 0; k < self->ctu_width * self->ctu_height; k++) {
    /* The ctu coordinate in the picture. */
    guint32 x = k % self->ctu_width;
    guint32 y = k / self->ctu_width;
    /* The ctu coordinate in the tile mode. */
    guint32 tile_x = 0;
    guint32 tile_y = 0;
    /* The index of the CTU in the tile mode. */
    guint32 tso = 0;

    for (i = 0; i < self->partition.num_tile_cols; i++)
      if (x >= ctu_tile_width_accu[i])
        tile_x = i;
    g_assert (tile_x <= self->partition.num_tile_cols - 1);

    for (j = 0; j < self->partition.num_tile_rows; j++)
      if (y >= ctu_tile_height_accu[j])
        tile_y = j;
    g_assert (tile_y <= self->partition.num_tile_rows - 1);

    /* add all ctus in the tiles the same line before us */
    for (i = 0; i < tile_x; i++)
      tso += self->partition.tile_ctu_rows[tile_y] *
          self->partition.tile_ctu_cols[i];

    /* add all ctus in the tiles above us */
    for (j = 0; j < tile_y; j++)
      tso += self->ctu_width * self->partition.tile_ctu_rows[j];

    /* add the ctus inside the same tile before us */
    tso += (y - ctu_tile_height_accu[tile_y]) *
        self->partition.tile_ctu_cols[tile_x]
        + x - ctu_tile_width_accu[tile_x];

    g_assert (tso < self->ctu_width * self->ctu_height);

    tile_slice_address_map[tso] = k;
  }

  for (i = 0; i < self->partition.num_slices; i++)
    self->partition.slice_segment_address[i] =
        tile_slice_address_map[tile_slice_address[i]];

  g_free (tile_slice_address);
  g_free (tile_slice_address_map);
}

static void
_h265_calculate_slice_partition (GstVaH265Enc * self, gint32 slice_structure)
{
  guint ctu_size;
  guint ctus_per_slice, ctus_mod_slice, cur_slice_ctus;
  guint last_ctu_index;
  guint i_slice;

  /* TODO: consider other slice structure modes */
  if (!(slice_structure & VA_ENC_SLICE_STRUCTURE_ARBITRARY_MACROBLOCKS) &&
      !(slice_structure & VA_ENC_SLICE_STRUCTURE_ARBITRARY_ROWS)) {
    GST_INFO_OBJECT (self, "Driver slice structure is %x, does not support"
        " ARBITRARY_MACROBLOCKS mode, fallback to no slice partition",
        slice_structure);
    self->partition.num_slices = 1;
  }

  self->partition.slice_segment_address =
      g_malloc (self->partition.num_slices * sizeof (guint32));
  self->partition.num_ctu_in_slice =
      g_malloc (self->partition.num_slices * sizeof (guint32));

  ctu_size = self->ctu_width * self->ctu_height;

  g_assert (self->partition.num_slices &&
      self->partition.num_slices < ctu_size);

  ctus_per_slice = ctu_size / self->partition.num_slices;
  ctus_mod_slice = ctu_size % self->partition.num_slices;
  last_ctu_index = 0;

  for (i_slice = 0; i_slice < self->partition.num_slices; i_slice++) {
    cur_slice_ctus = ctus_per_slice;
    /* Scatter the remainder to each slice */
    if (ctus_mod_slice) {
      ++cur_slice_ctus;
      --ctus_mod_slice;
    }

    /* Align start address to the row begin */
    if (slice_structure & VA_ENC_SLICE_STRUCTURE_ARBITRARY_ROWS) {
      guint ctu_width_round_factor;

      ctu_width_round_factor =
          self->ctu_width - (cur_slice_ctus % self->ctu_width);
      cur_slice_ctus += ctu_width_round_factor;
      if ((last_ctu_index + cur_slice_ctus) > ctu_size)
        cur_slice_ctus = ctu_size - last_ctu_index;
    }

    self->partition.slice_segment_address[i_slice] = last_ctu_index;
    self->partition.num_ctu_in_slice[i_slice] = cur_slice_ctus;

    /* set calculation for next slice */
    last_ctu_index += cur_slice_ctus;
    g_assert (last_ctu_index <= ctu_size);
  }
}

static gboolean
_h265_setup_slice_and_tile_partition (GstVaH265Enc * self)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  gint32 max_slices;
  gint32 slice_structure;

  /* Ensure the num_slices provided by the user not exceed the limit
   * of the number of slices permitted by the stream and by the
   * hardware. */
  g_assert (self->partition.num_slices >= 1);
  max_slices = gst_va_encoder_get_max_slice_num (base->encoder,
      base->profile, GST_VA_BASE_ENC_ENTRYPOINT (base));
  if (self->partition.num_slices > max_slices)
    self->partition.num_slices = max_slices;

  /* The stream size limit. */
  if (self->partition.num_slices >
      ((self->ctu_width * self->ctu_height + 1) / 2))
    self->partition.num_slices = ((self->ctu_width * self->ctu_height + 1) / 2);

  slice_structure = gst_va_encoder_get_slice_structure (base->encoder,
      base->profile, GST_VA_BASE_ENC_ENTRYPOINT (base));

  if (_is_tile_enabled (self)) {
    const GstVaH265LevelLimits *level_limits;
    guint i;

    if (!gst_va_encoder_has_tile (base->encoder,
            base->profile, GST_VA_BASE_ENC_ENTRYPOINT (base))) {
      self->partition.num_tile_cols = 1;
      self->partition.num_tile_rows = 1;
    }

    level_limits = NULL;
    for (i = 0; i < G_N_ELEMENTS (_va_h265_level_limits); i++) {
      if (_va_h265_level_limits[i].level_idc == self->level_idc) {
        level_limits = &_va_h265_level_limits[i];
        break;
      }
    }
    g_assert (level_limits);

    if (self->partition.num_tile_cols > level_limits->MaxTileColumns) {
      GST_INFO_OBJECT (self, "num_tile_cols:%d exceeds MaxTileColumns:%d"
          " of level %s", self->partition.num_tile_cols,
          level_limits->MaxTileColumns, self->level_str);
      self->partition.num_tile_cols = level_limits->MaxTileColumns;
    }
    if (self->partition.num_tile_rows > level_limits->MaxTileRows) {
      GST_INFO_OBJECT (self, "num_tile_rows:%d exceeds MaxTileRows:%d"
          " of level %s", self->partition.num_tile_rows,
          level_limits->MaxTileRows, self->level_str);
      self->partition.num_tile_rows = level_limits->MaxTileRows;
    }

    if (self->partition.num_tile_cols > self->ctu_width) {
      GST_INFO_OBJECT (self,
          "Only %d CTUs in width, not enough to split into %d tile columns",
          self->ctu_width, self->partition.num_tile_cols);
      self->partition.num_tile_cols = self->ctu_width;
    }
    if (self->partition.num_tile_rows > self->ctu_height) {
      GST_INFO_OBJECT (self,
          "Only %d CTUs in height, not enough to split into %d tile rows",
          self->ctu_height, self->partition.num_tile_rows);
      self->partition.num_tile_rows = self->ctu_height;
    }

    /* Some driver require that the slice should not span tiles,
       we need to increase slice number if needed. */
    if (gst_va_display_is_implementation (base->display,
            GST_VA_IMPLEMENTATION_INTEL_IHD)) {
      if (self->partition.num_slices <
          self->partition.num_tile_cols * self->partition.num_tile_rows) {
        if (self->partition.num_tile_cols * self->partition.num_tile_rows >
            max_slices) {
          GST_ERROR_OBJECT (self, "The slice can not span tiles, but total"
              " tile num %d is bigger than max_slices %d",
              self->partition.num_tile_cols * self->partition.num_tile_rows,
              max_slices);
          return FALSE;
        } else {
          GST_INFO_OBJECT (self, "The num_slices %d is smaller than tile"
              " num %d. The slice can not span tiles, so set the num-slices"
              " to tile num.", self->partition.num_slices,
              self->partition.num_tile_cols * self->partition.num_tile_rows);
          self->partition.num_slices =
              self->partition.num_tile_cols * self->partition.num_tile_rows;
        }
      }

      self->partition.slice_span_tiles = FALSE;
    } else {
      self->partition.slice_span_tiles = TRUE;
    }

    _h265_calculate_tile_partition (self);
  } else {
    _h265_calculate_slice_partition (self, slice_structure);
  }

  update_property_uint (base, &self->prop.num_slices,
      self->partition.num_slices, PROP_NUM_SLICES);
  update_property_uint (base, &self->prop.num_tile_cols,
      self->partition.num_tile_cols, PROP_NUM_TILE_COLS);
  update_property_uint (base, &self->prop.num_tile_rows,
      self->partition.num_tile_rows, PROP_NUM_TILE_ROWS);

  return TRUE;
}

/* Normalizes bitrate (and CPB size) for HRD conformance */
static void
_h265_calculate_bitrate_hrd (GstVaH265Enc * self)
{
  guint bitrate_bits, cpb_bits_size;

  /* Round down bitrate. This is a hard limit mandated by the user */
  g_assert (SX_BITRATE >= 6);
  bitrate_bits = (self->rc.max_bitrate * 1000) & ~((1U << SX_BITRATE) - 1);
  GST_DEBUG_OBJECT (self, "Max bitrate: %u bits/sec", bitrate_bits);
  self->rc.max_bitrate_bits = bitrate_bits;

  bitrate_bits = (self->rc.target_bitrate * 1000) & ~((1U << SX_BITRATE) - 1);
  GST_DEBUG_OBJECT (self, "Target bitrate: %u bits/sec", bitrate_bits);
  self->rc.target_bitrate_bits = bitrate_bits;

  if (self->rc.cpb_size > 0 && self->rc.cpb_size < (self->rc.max_bitrate / 2)) {
    GST_INFO_OBJECT (self, "Too small cpb_size: %d", self->rc.cpb_size);
    self->rc.cpb_size = 0;
  }

  if (self->rc.cpb_size == 0) {
    /* We cache 2 second coded data by default. */
    self->rc.cpb_size = self->rc.max_bitrate * 2;
    GST_INFO_OBJECT (self, "Adjust cpb_size to: %d", self->rc.cpb_size);
  }

  /* Round up CPB size. This is an HRD compliance detail */
  g_assert (SX_CPB_SIZE >= 4);
  cpb_bits_size = (self->rc.cpb_size * 1000) & ~((1U << SX_CPB_SIZE) - 1);

  GST_DEBUG_OBJECT (self, "HRD CPB size: %u bits", cpb_bits_size);
  self->rc.cpb_length_bits = cpb_bits_size;
}

/* Estimates a good enough bitrate if none was supplied */
static gboolean
_h265_ensure_rate_control (GstVaH265Enc * self)
{
  /* User can specify the properties of: "bitrate", "target-percentage",
   * "max-qp", "min-qp", "qpi", "qpp", "qpb", "mbbrc", "cpb-size",
   * "rate-control" and "target-usage" to control the RC behavior.
   *
   * "target-usage" is different from the others, it controls the encoding
   * speed and quality, while the others control encoding bit rate and
   * quality. The lower value has better quality(maybe bigger MV search
   * range) but slower speed, the higher value has faster speed but lower
   * quality.
   *
   * The possible composition to control the bit rate and quality:
   *
   * 1. CQP mode: "rate-control=cqp", then "qpi", "qpp" and "qpb"
   *    specify the QP of I/P/B frames respectively(within the
   *    "max-qp" and "min-qp" range). The QP will not change during
   *    the whole stream. Other properties are ignored.
   *
   * 2. CBR mode: "rate-control=CBR", then the "bitrate" specify the
   *    target bit rate and the "cpb-size" specifies the max coded
   *    picture buffer size to avoid overflow. If the "bitrate" is not
   *    set, it is calculated by the picture resolution and frame
   *    rate. If "cpb-size" is not set, it is set to the size of
   *    caching 2 second coded data. Encoder will try its best to make
   *    the QP with in the ["max-qp", "min-qp"] range. "mbbrc" can
   *    enable bit rate control in macro block level. Other paramters
   *    are ignored.
   *
   * 3. VBR mode: "rate-control=VBR", then the "bitrate" specify the
   *    target bit rate, "target-percentage" is used to calculate the
   *    max bit rate of VBR mode by ("bitrate" * 100) /
   *    "target-percentage". It is also used by driver to calculate
   *    the min bit rate. The "cpb-size" specifies the max coded
   *    picture buffer size to avoid overflow. If the "bitrate" is not
   *    set, the target bit rate will be calculated by the picture
   *    resolution and frame rate. Encoder will try its best to make
   *    the QP with in the ["max-qp", "min-qp"] range. "mbbrc" can
   *    enable bit rate control in macro block level. Other paramters
   *    are ignored.
   *
   * 4. VCM mode: "rate-control=VCM", then the "bitrate" specify the
   *    target bit rate, and encoder will try its best to make the QP
   *    with in the ["max-qp", "min-qp"] range. Other paramters are
   *    ignored.
   */

  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint bitrate;
  guint32 rc_mode, quality_level, rc_ctrl;

  quality_level = gst_va_encoder_get_quality_level (base->encoder,
      base->profile, GST_VA_BASE_ENC_ENTRYPOINT (base));
  if (self->rc.target_usage > quality_level) {
    GST_INFO_OBJECT (self, "User setting target-usage: %d is not supported, "
        "fallback to %d", self->rc.target_usage, quality_level);
    self->rc.target_usage = quality_level;
    update_property_uint (base, &self->prop.target_usage,
        self->rc.target_usage, PROP_TARGET_USAGE);
  }

  GST_OBJECT_LOCK (self);
  rc_ctrl = self->prop.rc_ctrl;
  GST_OBJECT_UNLOCK (self);

  if (rc_ctrl != VA_RC_NONE) {
    rc_mode = gst_va_encoder_get_rate_control_mode (base->encoder,
        base->profile, GST_VA_BASE_ENC_ENTRYPOINT (base));
    if (!(rc_mode & rc_ctrl)) {
      guint32 defval =
          G_PARAM_SPEC_ENUM (properties[PROP_RATE_CONTROL])->default_value;
      GST_INFO_OBJECT (self, "The rate control mode %s is not supported, "
          "fallback to %s mode", _rate_control_get_name (rc_ctrl),
          _rate_control_get_name (defval));
      self->rc.rc_ctrl_mode = defval;
      update_property_uint (base, &self->prop.rc_ctrl,
          self->rc.rc_ctrl_mode, PROP_RATE_CONTROL);
    }
  } else {
    self->rc.rc_ctrl_mode = VA_RC_NONE;
  }

  if (self->rc.min_qp > self->rc.max_qp) {
    GST_INFO_OBJECT (self, "The min_qp %d is bigger than the max_qp %d, "
        "set it to the max_qp", self->rc.min_qp, self->rc.max_qp);
    self->rc.min_qp = self->rc.max_qp;

    update_property_uint (base, &self->prop.min_qp, self->rc.min_qp,
        PROP_MIN_QP);
  }

  /* Make all the qp in the valid range */
  if (self->rc.qp_i < self->rc.min_qp) {
    if (self->rc.qp_i != 26)
      GST_INFO_OBJECT (self, "The qp_i %d is smaller than the min_qp %d, "
          "set it to the min_qp", self->rc.qp_i, self->rc.min_qp);
    self->rc.qp_i = self->rc.min_qp;
  }
  if (self->rc.qp_i > self->rc.max_qp) {
    if (self->rc.qp_i != 26)
      GST_INFO_OBJECT (self, "The qp_i %d is bigger than the max_qp %d, "
          "set it to the max_qp", self->rc.qp_i, self->rc.max_qp);
    self->rc.qp_i = self->rc.max_qp;
  }

  if (self->rc.qp_p < self->rc.min_qp) {
    if (self->rc.qp_p != 26)
      GST_INFO_OBJECT (self, "The qp_p %d is smaller than the min_qp %d, "
          "set it to the min_qp", self->rc.qp_p, self->rc.min_qp);
    self->rc.qp_p = self->rc.min_qp;
  }
  if (self->rc.qp_p > self->rc.max_qp) {
    if (self->rc.qp_p != 26)
      GST_INFO_OBJECT (self, "The qp_p %d is bigger than the max_qp %d, "
          "set it to the max_qp", self->rc.qp_p, self->rc.max_qp);
    self->rc.qp_p = self->rc.max_qp;
  }

  if (self->rc.qp_b < self->rc.min_qp) {
    if (self->rc.qp_b != 26)
      GST_INFO_OBJECT (self, "The qp_b %d is smaller than the min_qp %d, "
          "set it to the min_qp", self->rc.qp_b, self->rc.min_qp);
    self->rc.qp_b = self->rc.min_qp;
  }
  if (self->rc.qp_b > self->rc.max_qp) {
    if (self->rc.qp_b != 26)
      GST_INFO_OBJECT (self, "The qp_b %d is bigger than the max_qp %d, "
          "set it to the max_qp", self->rc.qp_b, self->rc.max_qp);
    self->rc.qp_b = self->rc.max_qp;
  }

  GST_OBJECT_LOCK (self);
  bitrate = self->prop.bitrate;
  GST_OBJECT_UNLOCK (self);

  /* Calculate a bitrate is not set. */
  if ((self->rc.rc_ctrl_mode == VA_RC_CBR || self->rc.rc_ctrl_mode == VA_RC_VBR
          || self->rc.rc_ctrl_mode == VA_RC_VCM) && bitrate == 0) {
    /* FIXME: Provide better estimation. */
    /* Choose the max value of all levels' MinCr which is 8, and x2 for
       conservative calculation. So just using a 1/16 compression ratio,
       and the bits per pixel for YUV420, YUV422, YUV444, accordingly. */
    guint64 factor;
    guint depth = 8, chrome = 1;
    guint bits_per_pix;

    if (!_h265_get_rtformat (self,
            GST_VIDEO_INFO_FORMAT (&base->in_info), &depth, &chrome))
      g_assert_not_reached ();

    if (chrome == 3) {
      bits_per_pix = 24;
    } else if (chrome == 2) {
      bits_per_pix = 16;
    } else {
      bits_per_pix = 12;
    }
    bits_per_pix = bits_per_pix + bits_per_pix * (depth - 8) / 8;

    factor = (guint64) self->luma_width * self->luma_height * bits_per_pix / 16;
    bitrate = gst_util_uint64_scale (factor,
        GST_VIDEO_INFO_FPS_N (&base->in_info),
        GST_VIDEO_INFO_FPS_D (&base->in_info)) / 1000;

    GST_INFO_OBJECT (self, "target bitrate computed to %u kbps", bitrate);

    update_property_uint (base, &self->prop.bitrate, bitrate, PROP_BITRATE);
  }

  /* Adjust the setting based on RC mode. */
  switch (self->rc.rc_ctrl_mode) {
    case VA_RC_NONE:
    case VA_RC_CQP:
      self->rc.max_bitrate = 0;
      self->rc.target_bitrate = 0;
      self->rc.target_percentage = 0;
      self->rc.cpb_size = 0;
      break;
    case VA_RC_CBR:
      self->rc.max_bitrate = bitrate;
      self->rc.target_bitrate = bitrate;
      self->rc.target_percentage = 100;
      self->rc.qp_i = self->rc.qp_p = self->rc.qp_b = 26;
      break;
    case VA_RC_VBR:
      g_assert (self->rc.target_percentage >= 10);
      self->rc.max_bitrate = (guint) gst_util_uint64_scale_int (bitrate,
          100, self->rc.target_percentage);
      self->rc.target_bitrate = bitrate;
      self->rc.qp_i = self->rc.qp_p = self->rc.qp_b = 26;
      break;
    case VA_RC_VCM:
      self->rc.max_bitrate = bitrate;
      self->rc.target_bitrate = bitrate;
      self->rc.target_percentage = 0;
      self->rc.qp_i = self->rc.qp_p = self->rc.qp_b = 26;
      self->rc.cpb_size = 0;

      if (self->gop.num_bframes > 0) {
        GST_INFO_OBJECT (self, "VCM mode just support I/P mode, no B frame");
        self->gop.num_bframes = 0;
        self->gop.b_pyramid = FALSE;
      }
      break;
    default:
      GST_WARNING_OBJECT (self, "Unsupported rate control");
      return FALSE;
      break;
  }

  GST_DEBUG_OBJECT (self, "Max bitrate: %u bits/sec, "
      "Target bitrate: %u bits/sec", self->rc.max_bitrate,
      self->rc.target_bitrate);

  if (self->rc.rc_ctrl_mode != VA_RC_NONE && self->rc.rc_ctrl_mode != VA_RC_CQP)
    _h265_calculate_bitrate_hrd (self);

  /* notifications */
  update_property_uint (base, &self->prop.min_qp, self->rc.min_qp, PROP_MIN_QP);
  update_property_uint (base, &self->prop.cpb_size,
      self->rc.cpb_size, PROP_CPB_SIZE);
  update_property_uint (base, &self->prop.target_percentage,
      self->rc.target_percentage, PROP_TARGET_PERCENTAGE);
  update_property_uint (base, &self->prop.qp_i, self->rc.qp_i, PROP_QP_I);
  update_property_uint (base, &self->prop.qp_p, self->rc.qp_p, PROP_QP_P);
  update_property_uint (base, &self->prop.qp_b, self->rc.qp_b, PROP_QP_B);

  return TRUE;
}

/* Derives the level and tier from the currently set limits */
static gboolean
_h265_calculate_tier_level (GstVaH265Enc * self)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint i, PicSizeInSamplesY, LumaSr;
  guint32 tier_max_bitrate;

  PicSizeInSamplesY = self->luma_width * self->luma_height;
  LumaSr = gst_util_uint64_scale_int_ceil (PicSizeInSamplesY,
      GST_VIDEO_INFO_FPS_N (&base->in_info),
      GST_VIDEO_INFO_FPS_D (&base->in_info));

  for (i = 0; i < G_N_ELEMENTS (_va_h265_level_limits); i++) {
    const GstVaH265LevelLimits *const limits = &_va_h265_level_limits[i];

    /* Choose level by luma picture size and luma sample rate */
    if (PicSizeInSamplesY <= limits->MaxLumaPs && LumaSr <= limits->MaxLumaSr)
      break;
  }

  if (i == G_N_ELEMENTS (_va_h265_level_limits))
    goto error_unsupported_level;

  self->level_idc = _va_h265_level_limits[i].level_idc;
  self->level_str = _va_h265_level_limits[i].level_name;
  self->min_cr = _va_h265_level_limits[i].MinCr;

  if (self->rc.rc_ctrl_mode == VA_RC_CQP) {
    g_assert (self->rc.max_bitrate == 0);

    /* We may need to calculate some max bit rate for CQP mode.
       Just set the main tier now. */
    self->tier_flag = FALSE;
  } else {
    if (_va_h265_level_limits[i].MaxBRTierHigh == 0 ||
        self->rc.max_bitrate <= _va_h265_level_limits[i].MaxBRTierMain) {
      self->tier_flag = FALSE;
    } else {
      self->tier_flag = TRUE;
    }
  }

  tier_max_bitrate = self->tier_flag ? _va_h265_level_limits[i].MaxBRTierHigh :
      _va_h265_level_limits[i].MaxBRTierMain;

  if (self->rc.max_bitrate > tier_max_bitrate) {
    GST_INFO_OBJECT (self, "The max bitrate of the stream is %u kbps, still"
        " larger than %s profile %s level %s tier's max bit rate %d kbps",
        self->rc.max_bitrate, gst_va_profile_name (base->profile),
        _va_h265_level_limits[i].level_name,
        (self->tier_flag ? "high" : "main"), tier_max_bitrate);
  }

  GST_DEBUG_OBJECT (self, "profile: %s, level: %s, tier :%s, MinCr: %d",
      gst_va_profile_name (base->profile), _va_h265_level_limits[i].level_name,
      (self->tier_flag ? "high" : "main"), self->min_cr);

  return TRUE;

error_unsupported_level:
  {
    GST_ERROR_OBJECT (self,
        "failed to find a suitable level matching codec config");
    return FALSE;
  }
}

struct PyramidInfo
{
  guint level;
  gint left_ref_poc_diff;
  gint right_ref_poc_diff;
};

static void
_set_pyramid_info (struct PyramidInfo *info, guint len,
    guint current_level, guint highest_level)
{
  guint index;

  g_assert (len >= 1);

  if (current_level == highest_level || len == 1) {
    for (index = 0; index < len; index++) {
      info[index].level = current_level;
      info[index].left_ref_poc_diff = -(index + 1);
      info[index].right_ref_poc_diff = len - index;
    }

    return;
  }

  index = len / 2;
  info[index].level = current_level;
  info[index].left_ref_poc_diff = -(index + 1);
  info[index].right_ref_poc_diff = len - index;

  current_level++;

  if (index > 0)
    _set_pyramid_info (info, index, current_level, highest_level);

  if (index + 1 < len)
    _set_pyramid_info (&info[index + 1], len - (index + 1),
        current_level, highest_level);
}

static void
_h265_create_gop_frame_types (GstVaH265Enc * self)
{
  guint i;
  guint i_frames = self->gop.num_iframes;
  struct PyramidInfo pyramid_info[31] = { 0, };

  if (self->gop.highest_pyramid_level > 0) {
    g_assert (self->gop.num_bframes > 0);
    _set_pyramid_info (pyramid_info, self->gop.num_bframes,
        0, self->gop.highest_pyramid_level);
  }

  g_assert (self->gop.idr_period <= MAX_GOP_SIZE);
  for (i = 0; i < self->gop.idr_period; i++) {
    if (i == 0) {
      self->gop.frame_types[i].slice_type = GST_H265_I_SLICE;
      self->gop.frame_types[i].is_ref = TRUE;
      continue;
    }

    /* Intra only stream. */
    if (self->gop.ip_period == 0) {
      self->gop.frame_types[i].slice_type = GST_H265_I_SLICE;
      self->gop.frame_types[i].is_ref = FALSE;
      continue;
    }

    if (i % self->gop.ip_period) {
      guint pyramid_index =
          i % self->gop.ip_period - 1 /* The first P or IDR */ ;

      self->gop.frame_types[i].slice_type = GST_H265_B_SLICE;
      self->gop.frame_types[i].pyramid_level =
          pyramid_info[pyramid_index].level;
      self->gop.frame_types[i].is_ref =
          (self->gop.frame_types[i].pyramid_level <
          self->gop.highest_pyramid_level);
      self->gop.frame_types[i].left_ref_poc_diff =
          pyramid_info[pyramid_index].left_ref_poc_diff;
      self->gop.frame_types[i].right_ref_poc_diff =
          pyramid_info[pyramid_index].right_ref_poc_diff;
      continue;
    }

    if (self->gop.i_period && i % self->gop.i_period == 0 && i_frames > 0) {
      /* Replace P with I. */
      self->gop.frame_types[i].slice_type = GST_H265_I_SLICE;
      self->gop.frame_types[i].is_ref = TRUE;
      i_frames--;
      continue;
    }

    self->gop.frame_types[i].slice_type = GST_H265_P_SLICE;
    self->gop.frame_types[i].is_ref = TRUE;
  }

  /* Force the last one to be a P */
  if (self->gop.idr_period > 1 && self->gop.ip_period > 0) {
    self->gop.frame_types[self->gop.idr_period - 1].slice_type =
        GST_H265_P_SLICE;
    self->gop.frame_types[self->gop.idr_period - 1].is_ref = TRUE;
  }
}

static void
_h265_print_gop_structure (GstVaH265Enc * self)
{
#ifndef GST_DISABLE_GST_DEBUG
  GString *str;
  gint i;

  if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) < GST_LEVEL_INFO)
    return;

  str = g_string_new (NULL);

  g_string_append_printf (str, "[ ");

  for (i = 0; i < self->gop.idr_period; i++) {
    if (i == 0) {
      g_string_append_printf (str, "IDR");
      continue;
    } else {
      g_string_append_printf (str, ", ");
    }

    if (self->gop.low_delay_b_mode &&
        self->gop.frame_types[i].slice_type == GST_H265_P_SLICE) {
      g_string_append_printf (str, "%s", "LDB");
    } else {
      g_string_append_printf (str, "%s",
          _h265_slice_type_name (self->gop.frame_types[i].slice_type));
    }

    if (self->gop.b_pyramid
        && self->gop.frame_types[i].slice_type == GST_H265_B_SLICE) {
      g_string_append_printf (str, "<L%d (%d, %d)>",
          self->gop.frame_types[i].pyramid_level,
          self->gop.frame_types[i].left_ref_poc_diff,
          self->gop.frame_types[i].right_ref_poc_diff);
    }

    if (self->gop.frame_types[i].is_ref) {
      g_string_append_printf (str, "(ref)");
    }

  }

  g_string_append_printf (str, " ]");

  GST_INFO_OBJECT (self, "GOP size: %d, forward reference %d, backward"
      " reference %d, GOP structure: %s", self->gop.idr_period,
      self->gop.forward_ref_num, self->gop.backward_ref_num, str->str);

  g_string_free (str, TRUE);
#endif
}

static void
_h265_calculate_coded_size (GstVaH265Enc * self)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint codedbuf_size = 0;
  guint chrome, depth;

  if (!_h265_get_rtformat (self,
          GST_VIDEO_INFO_FORMAT (&base->in_info), &depth, &chrome))
    g_assert_not_reached ();

  switch (chrome) {
    case 0:
      /* 4:0:0 */
    case 1:
      /* 4:2:0 */
      codedbuf_size = (self->luma_width * self->luma_height * 3 / 2);
      break;
    case 2:
      /* 4:2:2 */
      codedbuf_size = (self->luma_width * self->luma_height * 2);
      break;
    case 3:
      /* 4:4:4 */
      codedbuf_size = (self->luma_width * self->luma_height * 3);
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  codedbuf_size = codedbuf_size + (codedbuf_size * (depth - 8) / 8);
  codedbuf_size = codedbuf_size / (self->min_cr / 2 /* For safety */ );

  /* FIXME: Using only a rough approximation for bitstream headers.
   * Not taken into account: ScalingList, RefPicListModification,
   * PredWeightTable, which is not used now. */
  /* Calculate the maximum sizes for common headers (in bits) */

  /* Account for VPS header */
  codedbuf_size += 4 /* start code */  + GST_ROUND_UP_8 (MAX_VPS_HDR_SIZE +
      MAX_PROFILE_TIER_LEVEL_SIZE + MAX_HRD_PARAMS_SIZE) / 8;

  /* Account for SPS header */
  codedbuf_size += 4 + GST_ROUND_UP_8 (MAX_SPS_HDR_SIZE +
      MAX_PROFILE_TIER_LEVEL_SIZE + 64 * MAX_SHORT_TERM_REFPICSET_SIZE +
      MAX_VUI_PARAMS_SIZE + MAX_HRD_PARAMS_SIZE) / 8;

  /* Account for PPS header */
  codedbuf_size += 4 + GST_ROUND_UP_8 (MAX_PPS_HDR_SIZE) / 8;

  /* Account for slice header */
  codedbuf_size += self->partition.num_slices * (4 +
      GST_ROUND_UP_8 (MAX_SLICE_HDR_SIZE + MAX_SHORT_TERM_REFPICSET_SIZE) / 8);

  base->codedbuf_size = codedbuf_size;
  GST_INFO_OBJECT (self, "Calculate codedbuf size: %u", base->codedbuf_size);
}

/* Get log2_max_frame_num_minus4, log2_max_pic_order_cnt_lsb_minus4
 * value, shall be in the range of 0 to 12, inclusive. */
static guint
_get_log2_max_num (guint num)
{
  guint ret = 0;

  while (num) {
    ++ret;
    num >>= 1;
  }

  /* shall be in the range of 0+4 to 12+4, inclusive. */
  if (ret < 4) {
    ret = 4;
  } else if (ret > 16) {
    ret = 16;
  }
  return ret;
}

/* Consider the idr_period, num_bframes, L0/L1 reference number.
 * TODO: Load some preset fixed GOP structure.
 * TODO: Skip this if in lookahead mode. */
static gboolean
_h265_generate_gop_structure (GstVaH265Enc * self)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint32 log2_max_frame_num;
  guint32 list0, list1, forward_num, backward_num, gop_ref_num;
  gint32 p_frames;
  guint32 prediction_direction;

  /* If not set, generate a idr every second */
  if (self->gop.idr_period == 0) {
    self->gop.idr_period = (GST_VIDEO_INFO_FPS_N (&base->in_info)
        + GST_VIDEO_INFO_FPS_D (&base->in_info) - 1) /
        GST_VIDEO_INFO_FPS_D (&base->in_info);
  }

  /* Do not use a too huge GOP size. */
  if (self->gop.idr_period > 1024) {
    self->gop.idr_period = 1024;
    GST_INFO_OBJECT (self, "Lowering the GOP size to %d", self->gop.idr_period);
  }

  update_property_uint (base, &self->prop.key_int_max, self->gop.idr_period,
      PROP_KEY_INT_MAX);

  /* Prefer have more than 1 refs for the GOP which is not very small. */
  if (self->gop.idr_period > 8) {
    if (self->gop.num_bframes > (self->gop.idr_period - 1) / 2) {
      self->gop.num_bframes = (self->gop.idr_period - 1) / 2;
      GST_INFO_OBJECT (self, "Lowering the number of num_bframes to %d",
          self->gop.num_bframes);
    }
  } else {
    /* beign and end should be ref */
    if (self->gop.num_bframes > self->gop.idr_period - 1 - 1) {
      if (self->gop.idr_period > 1) {
        self->gop.num_bframes = self->gop.idr_period - 1 - 1;
      } else {
        self->gop.num_bframes = 0;
      }
      GST_INFO_OBJECT (self, "Lowering the number of num_bframes to %d",
          self->gop.num_bframes);
    }
  }

  if (!gst_va_encoder_get_max_num_reference (base->encoder, base->profile,
          GST_VA_BASE_ENC_ENTRYPOINT (base), &list0, &list1)) {
    GST_INFO_OBJECT (self, "Failed to get the max num reference");
    list0 = 1;
    list1 = 0;
  }
  self->gop.max_l0_num = list0;
  self->gop.max_l1_num = list1;
  GST_DEBUG_OBJECT (self, "list0 num: %d, list1 num: %d",
      self->gop.max_l0_num, self->gop.max_l1_num);

  forward_num = list0;
  backward_num = list1;

  prediction_direction = gst_va_encoder_get_prediction_direction (base->encoder,
      base->profile, GST_VA_BASE_ENC_ENTRYPOINT (base));
  if (prediction_direction) {
    if (!(prediction_direction & VA_PREDICTION_DIRECTION_PREVIOUS)) {
      GST_INFO_OBJECT (self, "No forward prediction support");
      forward_num = 0;
      /* Only backward ref is insane. */
      backward_num = 0;
    }

    if (!(prediction_direction & VA_PREDICTION_DIRECTION_FUTURE)) {
      GST_INFO_OBJECT (self, "No backward prediction support");
      backward_num = 0;
    }

    if (prediction_direction & VA_PREDICTION_DIRECTION_BI_NOT_EMPTY) {
      if (self->gop.max_l1_num == 0) {
        GST_INFO_OBJECT (self, "Not possible to support "
            "VA_PREDICTION_DIRECTION_BI_NOT_EMPTY while list1 is 0");
        return FALSE;
      }
      GST_INFO_OBJECT (self, "Enable low-delay-b mode");
      self->gop.low_delay_b_mode = TRUE;
    }
  }

  if (forward_num > self->gop.num_ref_frames)
    forward_num = self->gop.num_ref_frames;
  if (backward_num > self->gop.num_ref_frames)
    backward_num = self->gop.num_ref_frames;

  if (forward_num == 0) {
    GST_INFO_OBJECT (self,
        "No reference support, fallback to intra only stream");

    /* It does not make sense that if only the list1 exists. */
    self->gop.num_ref_frames = 0;

    self->gop.ip_period = 0;
    self->gop.num_bframes = 0;
    self->gop.b_pyramid = FALSE;
    self->gop.highest_pyramid_level = 0;
    self->gop.num_iframes = self->gop.idr_period - 1 /* The idr */ ;
    self->gop.forward_ref_num = 0;
    self->gop.backward_ref_num = 0;
    goto create_poc;
  }

  if (self->gop.num_ref_frames <= 1) {
    GST_INFO_OBJECT (self, "The number of reference frames is only %d,"
        " no B frame allowed, fallback to I/P mode", self->gop.num_ref_frames);
    self->gop.num_bframes = 0;
    backward_num = 0;
  }

  /* b_pyramid needs at least 1 ref for B, besides the I/P */
  if (self->gop.b_pyramid && self->gop.num_ref_frames <= 2) {
    GST_INFO_OBJECT (self, "The number of reference frames is only %d,"
        " not enough for b_pyramid", self->gop.num_ref_frames);
    self->gop.b_pyramid = FALSE;
  }

  if (backward_num == 0 && self->gop.num_bframes > 0) {
    GST_INFO_OBJECT (self,
        "No hw reference support for list 1, fallback to I/P mode");
    self->gop.num_bframes = 0;
    self->gop.b_pyramid = FALSE;
  }

  /* I/P mode, no list1 needed. */
  if (self->gop.num_bframes == 0)
    backward_num = 0;

  /* Not enough B frame, no need for b_pyramid. */
  if (self->gop.num_bframes <= 1)
    self->gop.b_pyramid = FALSE;

  if (self->gop.num_ref_frames > forward_num + backward_num) {
    self->gop.num_ref_frames = forward_num + backward_num;
    GST_INFO_OBJECT (self, "HW limits, lowering the number of reference"
        " frames to %d", self->gop.num_ref_frames);
  }
  self->gop.num_ref_frames = MIN (self->gop.num_ref_frames, 15);

  /* How many possible refs within a GOP. */
  gop_ref_num = (self->gop.idr_period + self->gop.num_bframes) /
      (self->gop.num_bframes + 1);
  /* The end ref */
  if (self->gop.num_bframes > 0
      /* frame_num % (self->gop.num_bframes + 1) happens to be the end P */
      && (self->gop.idr_period % (self->gop.num_bframes + 1) != 1))
    gop_ref_num++;

  /* Adjust reference num based on B frames and B pyramid. */
  if (self->gop.num_bframes == 0) {
    self->gop.b_pyramid = FALSE;
    self->gop.forward_ref_num = self->gop.num_ref_frames;
    self->gop.backward_ref_num = 0;
  } else if (self->gop.b_pyramid) {
    guint b_frames = self->gop.num_bframes;
    guint b_refs;

    /* set b pyramid one backward ref. */
    self->gop.backward_ref_num = 1;
    self->gop.forward_ref_num =
        self->gop.num_ref_frames - self->gop.backward_ref_num;
    if (self->gop.forward_ref_num > forward_num)
      self->gop.forward_ref_num = forward_num;

    /* Balance the forward and backward refs */
    if ((self->gop.forward_ref_num > self->gop.backward_ref_num * 3)
        && backward_num > 1) {
      self->gop.backward_ref_num++;

      self->gop.forward_ref_num =
          self->gop.num_ref_frames - self->gop.backward_ref_num;
      if (self->gop.forward_ref_num > forward_num)
        self->gop.forward_ref_num = forward_num;
    }

    b_frames = b_frames / 2;
    b_refs = 0;
    while (b_frames) {
      /* At least 1 B ref for each level, plus begin and end 2 P/I */
      b_refs += 1;
      if (b_refs + 2 > self->gop.num_ref_frames)
        break;

      self->gop.highest_pyramid_level++;
      b_frames = b_frames / 2;
    }

    GST_INFO_OBJECT (self, "pyramid level is %d",
        self->gop.highest_pyramid_level);
  } else {
    /* We prefer list0. Backward refs have more latency. */
    self->gop.backward_ref_num = 1;
    self->gop.forward_ref_num =
        self->gop.num_ref_frames - self->gop.backward_ref_num;
    /* Balance the forward and backward refs, but not cause a big latency. */
    while ((self->gop.num_bframes * self->gop.backward_ref_num <= 16)
        && (self->gop.backward_ref_num <= gop_ref_num)
        && (self->gop.backward_ref_num < backward_num)
        && (self->gop.forward_ref_num / self->gop.backward_ref_num > 4)) {
      self->gop.forward_ref_num--;
      self->gop.backward_ref_num++;
    }

    if (self->gop.forward_ref_num > forward_num)
      self->gop.forward_ref_num = forward_num;
  }

  /* It's OK, keep slots for GST_VIDEO_CODEC_FRAME_IS_FORCE_KEYFRAME frame. */
  if (self->gop.forward_ref_num > gop_ref_num)
    GST_DEBUG_OBJECT (self, "num_ref_frames %d is bigger than gop_ref_num %d",
        self->gop.forward_ref_num, gop_ref_num);

  /* Include the ref picture itself. */
  self->gop.ip_period = 1 + self->gop.num_bframes;

  p_frames = gop_ref_num - 1 /* IDR */ ;
  if (p_frames < 0)
    p_frames = 0;
  if (self->gop.num_iframes > p_frames) {
    self->gop.num_iframes = p_frames;
    GST_INFO_OBJECT (self, "Too many I frames insertion, lowering it to %d",
        self->gop.num_iframes);
  }

  if (self->gop.num_iframes > 0) {
    guint total_i_frames = self->gop.num_iframes + 1 /* IDR */ ;
    self->gop.i_period =
        (gop_ref_num / total_i_frames) * (self->gop.num_bframes + 1);
  }

create_poc:
  /* init max_frame_num, max_poc */
  log2_max_frame_num = _get_log2_max_num (self->gop.idr_period);
  self->gop.log2_max_pic_order_cnt = log2_max_frame_num;
  self->gop.max_pic_order_cnt = 1 << self->gop.log2_max_pic_order_cnt;
  self->gop.num_reorder_frames = self->gop.b_pyramid ?
      self->gop.highest_pyramid_level * 2 + 1 /* the last P frame. */ :
      self->gop.backward_ref_num;
  /* Should not exceed the max ref num. */
  self->gop.num_reorder_frames =
      MIN (self->gop.num_reorder_frames, self->gop.num_ref_frames);
  self->gop.num_reorder_frames = MIN (self->gop.num_reorder_frames, 16);
  self->gop.max_dpb_size = self->gop.num_ref_frames + 1;

  _h265_create_gop_frame_types (self);
  _h265_print_gop_structure (self);

  /* notifications */
  update_property_uint (base, &self->prop.num_ref_frames,
      self->gop.num_ref_frames, PROP_NUM_REF_FRAMES);
  update_property_uint (base, &self->prop.num_iframes,
      self->gop.num_iframes, PROP_IFRAMES);
  update_property_uint (base, &self->prop.num_bframes,
      self->gop.num_bframes, PROP_BFRAMES);
  update_property_bool (base, &self->prop.b_pyramid,
      self->gop.b_pyramid, PROP_B_PYRAMID);

  return TRUE;
}

static gboolean
_h265_init_packed_headers (GstVaH265Enc * self)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint32 packed_headers;
  guint32 desired_packed_headers = VA_ENC_PACKED_HEADER_SEQUENCE        /* SPS */
      | VA_ENC_PACKED_HEADER_PICTURE    /* PPS */
      | VA_ENC_PACKED_HEADER_SLICE      /* Slice headers */
      | VA_ENC_PACKED_HEADER_RAW_DATA;  /* SEI, AUD, etc. */

  self->packed_headers = 0;

  if (!gst_va_encoder_get_packed_headers (base->encoder, base->profile,
          GST_VA_BASE_ENC_ENTRYPOINT (base), &packed_headers))
    return FALSE;

  if (desired_packed_headers & ~packed_headers) {
    GST_INFO_OBJECT (self, "Driver does not support some wanted packed headers "
        "(wanted %#x, found %#x)", desired_packed_headers, packed_headers);
  }

  self->packed_headers = desired_packed_headers & packed_headers;

  return TRUE;
}

static guint
_get_chroma_format_idc (guint va_chroma)
{
  guint chroma_format_idc;

  switch (va_chroma) {
    case VA_RT_FORMAT_YUV400:
      chroma_format_idc = 0;
      break;
    case VA_RT_FORMAT_YUV420:
    case VA_RT_FORMAT_YUV420_10:
    case VA_RT_FORMAT_YUV420_12:
      chroma_format_idc = 1;
      break;
    case VA_RT_FORMAT_YUV422:
    case VA_RT_FORMAT_YUV422_10:
    case VA_RT_FORMAT_YUV422_12:
      chroma_format_idc = 2;
      break;
    case VA_RT_FORMAT_YUV444:
    case VA_RT_FORMAT_YUV444_10:
    case VA_RT_FORMAT_YUV444_12:
      chroma_format_idc = 3;
      break;
    default:
      GST_WARNING ("unsupported VA chroma value");
      chroma_format_idc = 1;
      break;
  }

  return chroma_format_idc;
}

static void
_h265_init_mvp (GstVaH265Enc * self, gboolean enable)
{
  if (enable) {
    /* For the simplicity, we only let MVP refer to List0[0],
       which is the last ref frame before the current frame. */
    self->features.temporal_mvp_enabled_flag = TRUE;
    self->features.collocated_from_l0_flag = TRUE;
    self->features.collocated_ref_idx = 0;
  } else {
    self->features.temporal_mvp_enabled_flag = FALSE;
    self->features.collocated_from_l0_flag = FALSE;
    self->features.collocated_ref_idx = 0xff;
  }
}

/* We need to decide the profile and entrypoint before call this.
   It applies the optimized features provided by the va driver. */
static void
_h265_setup_encoding_features (GstVaH265Enc * self)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);

#if VA_CHECK_VERSION(1, 13, 0)
  VAConfigAttribValEncHEVCFeatures features;
  VAStatus status;
  VAConfigAttrib attrib = {.type = VAConfigAttribEncHEVCFeatures };

  status = vaGetConfigAttributes (gst_va_display_get_va_dpy (base->display),
      base->profile, GST_VA_BASE_ENC_ENTRYPOINT (base), &attrib, 1);
  if (status != VA_STATUS_SUCCESS) {
    GST_INFO_OBJECT (self, "Failed to query encoding features: %s",
        vaErrorStr (status));
    goto default_options;
  }

  if (attrib.value == VA_ATTRIB_NOT_SUPPORTED) {
    GST_INFO_OBJECT (self, "Driver does not support query encoding features");
    goto default_options;
  }

  features.value = attrib.value;

  /* We do not enable this no matter what the driver say. */
  self->features.separate_colour_plane_flag = FALSE;
  self->features.colour_plane_id = 0;

  /* We do not enable scaling_list now. */
  self->features.scaling_list_enabled_flag = FALSE;
  self->features.scaling_list_data_present_flag = FALSE;

  self->features.amp_enabled_flag = (features.bits.amp != 0);

  self->features.sample_adaptive_offset_enabled_flag = (features.bits.sao != 0);
  self->features.slice_sao_luma_flag = (features.bits.sao != 0);
  self->features.slice_sao_chroma_flag = (features.bits.sao != 0);

  self->features.pcm_enabled_flag = (features.bits.pcm != 0);
  if (!self->features.pcm_enabled_flag) {
    self->features.pcm_sample_bit_depth_luma_minus1 = 0;
    self->features.pcm_sample_bit_depth_chroma_minus1 = 0;
    self->features.log2_min_pcm_luma_coding_block_size_minus3 = 0;
    self->features.log2_max_pcm_luma_coding_block_size_minus3 = 0;
  } else {
    self->features.pcm_sample_bit_depth_luma_minus1 =
        self->bits_depth_luma_minus8 + 8 - 1;
    self->features.pcm_sample_bit_depth_chroma_minus1 =
        self->bits_depth_chroma_minus8 + 8 - 1;
    /* log2_min_pcm_luma_coding_block_size_minus3 and
       log2_diff_max_min_pcm_luma_coding_block_size set
       in coding_block_size */
  }
  self->features.pcm_loop_filter_disabled_flag = FALSE;

  _h265_init_mvp (self, features.bits.temporal_mvp != 0);

  self->features.strong_intra_smoothing_enabled_flag =
      (features.bits.strong_intra_smoothing != 0);

  /* TODO: dependent slice */
  self->features.dependent_slice_segment_flag = FALSE;

  self->features.sign_data_hiding_enabled_flag =
      (features.bits.sign_data_hiding != 0);

  self->features.constrained_intra_pred_flag =
      (features.bits.constrained_intra_pred != 0);

  self->features.transform_skip_enabled_flag =
      (features.bits.transform_skip != 0);

  if (self->rc.rc_ctrl_mode != VA_RC_CQP)
    self->features.cu_qp_delta_enabled_flag = !!features.bits.cu_qp_delta;
  else
    self->features.cu_qp_delta_enabled_flag = 0;

  if (self->features.cu_qp_delta_enabled_flag) {
    self->features.diff_cu_qp_delta_depth =
        self->features.log2_diff_max_min_luma_coding_block_size;
  }

  /* TODO: use weighted pred */
  self->features.weighted_pred_flag = FALSE;
  self->features.weighted_bipred_flag = FALSE;

  self->features.transquant_bypass_enabled_flag =
      (features.bits.transquant_bypass != 0);
  goto print_options;

default_options:
#endif

  GST_DEBUG_OBJECT (self, "Apply default setting for features");

  self->features.separate_colour_plane_flag = FALSE;
  self->features.colour_plane_id = 0;
  self->features.scaling_list_enabled_flag = FALSE;
  self->features.scaling_list_data_present_flag = FALSE;
  self->features.amp_enabled_flag = TRUE;
  self->features.sample_adaptive_offset_enabled_flag = FALSE;
  self->features.slice_sao_luma_flag = FALSE;
  self->features.slice_sao_chroma_flag = FALSE;
  self->features.pcm_enabled_flag = 0;
  self->features.pcm_sample_bit_depth_luma_minus1 = 0;
  self->features.pcm_sample_bit_depth_chroma_minus1 = 0;
  self->features.log2_min_pcm_luma_coding_block_size_minus3 = 0;
  self->features.log2_max_pcm_luma_coding_block_size_minus3 = 0;
  self->features.pcm_loop_filter_disabled_flag = FALSE;
  _h265_init_mvp (self, TRUE);
  self->features.strong_intra_smoothing_enabled_flag = TRUE;
  self->features.dependent_slice_segment_flag = FALSE;
  self->features.sign_data_hiding_enabled_flag = FALSE;
  self->features.constrained_intra_pred_flag = FALSE;
  self->features.transform_skip_enabled_flag = TRUE;
  self->features.cu_qp_delta_enabled_flag =
      (self->rc.rc_ctrl_mode != VA_RC_CQP);
  self->features.diff_cu_qp_delta_depth = 0;
  self->features.weighted_pred_flag = FALSE;
  self->features.weighted_bipred_flag = FALSE;
  self->features.transquant_bypass_enabled_flag = FALSE;

#if VA_CHECK_VERSION(1, 13, 0)
print_options:
#endif
  GST_DEBUG_OBJECT (self, "Set features to: "
      "separate_colour_plane_flag = %d, "
      "colour_plane_id = %d, "
      "scaling_list_enabled_flag = %d, "
      "scaling_list_data_present_flag = %d, "
      "amp_enabled_flag = %d, "
      "sample_adaptive_offset_enabled_flag = %d, "
      "slice_sao_luma_flag = %d, "
      "slice_sao_chroma_flag = %d, "
      "pcm_enabled_flag = %d, "
      "pcm_sample_bit_depth_luma_minus1 = %d, "
      "pcm_sample_bit_depth_chroma_minus1 = %d, "
      "log2_min_pcm_luma_coding_block_size_minus3 = %d, "
      "log2_max_pcm_luma_coding_block_size_minus3 = %d, "
      "pcm_loop_filter_disabled_flag = %d, "
      "temporal_mvp_enabled_flag = %d, "
      "collocated_from_l0_flag = %d, "
      "collocated_ref_idx = %d, "
      "strong_intra_smoothing_enabled_flag = %d, "
      "dependent_slice_segment_flag = %d, "
      "sign_data_hiding_enabled_flag = %d, "
      "constrained_intra_pred_flag = %d, "
      "transform_skip_enabled_flag = %d, "
      "cu_qp_delta_enabled_flag = %d, "
      "diff_cu_qp_delta_depth = %d, "
      "weighted_pred_flag = %d, "
      "weighted_bipred_flag = %d, "
      "transquant_bypass_enabled_flag = %d",
      self->features.separate_colour_plane_flag,
      self->features.colour_plane_id,
      self->features.scaling_list_enabled_flag,
      self->features.scaling_list_data_present_flag,
      self->features.amp_enabled_flag,
      self->features.sample_adaptive_offset_enabled_flag,
      self->features.slice_sao_luma_flag,
      self->features.slice_sao_chroma_flag,
      self->features.pcm_enabled_flag,
      self->features.pcm_sample_bit_depth_luma_minus1,
      self->features.pcm_sample_bit_depth_chroma_minus1,
      self->features.log2_min_pcm_luma_coding_block_size_minus3,
      self->features.log2_max_pcm_luma_coding_block_size_minus3,
      self->features.pcm_loop_filter_disabled_flag,
      self->features.temporal_mvp_enabled_flag,
      self->features.collocated_from_l0_flag,
      self->features.collocated_ref_idx,
      self->features.strong_intra_smoothing_enabled_flag,
      self->features.dependent_slice_segment_flag,
      self->features.sign_data_hiding_enabled_flag,
      self->features.constrained_intra_pred_flag,
      self->features.transform_skip_enabled_flag,
      self->features.cu_qp_delta_enabled_flag,
      self->features.diff_cu_qp_delta_depth,
      self->features.weighted_pred_flag,
      self->features.weighted_bipred_flag,
      self->features.transquant_bypass_enabled_flag);

  /* Ensure trellis. */
  if (self->features.use_trellis &&
      !gst_va_encoder_has_trellis (base->encoder, base->profile,
          GST_VA_BASE_ENC_ENTRYPOINT (base))) {
    GST_INFO_OBJECT (self, "The trellis is not supported");
    self->features.use_trellis = FALSE;
  }

  update_property_bool (base, &self->prop.use_trellis,
      self->features.use_trellis, PROP_TRELLIS);
}

/* We need to decide the profile and entrypoint before call this.
   It applies the optimized block size(coding and tranform) provided
   by the va driver. */
static void
_h265_set_coding_block_size (GstVaH265Enc * self)
{
#if VA_CHECK_VERSION(1, 13, 0)
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);

  VAConfigAttribValEncHEVCBlockSizes block_size;
  VAStatus status;
  VAConfigAttrib attrib = {.type = VAConfigAttribEncHEVCBlockSizes };

  status = vaGetConfigAttributes (gst_va_display_get_va_dpy (base->display),
      base->profile, GST_VA_BASE_ENC_ENTRYPOINT (base), &attrib, 1);
  if (status != VA_STATUS_SUCCESS) {
    GST_INFO_OBJECT (self, "Failed to query coding block size: %s",
        vaErrorStr (status));
    goto default_setting;
  }

  if (attrib.value == VA_ATTRIB_NOT_SUPPORTED) {
    GST_INFO_OBJECT (self, "Driver does not support query"
        " coding block size");
    goto default_setting;
  }

  block_size.value = attrib.value;

  /* We always choose the biggest coding block size and the biggest
     hierarchy depth to achieve the best compression result.
     TODO: May choose smaller value when fast encoding is needed. */

  if (block_size.bits.log2_min_luma_coding_block_size_minus3 >
      block_size.bits.log2_max_coding_tree_block_size_minus3) {
    GST_WARNING_OBJECT (self, "Invalid log2_min_luma_coding_block_size_minus3:"
        " %d, bigger than log2_max_coding_tree_block_size_minus3: %d",
        block_size.bits.log2_min_luma_coding_block_size_minus3,
        block_size.bits.log2_max_coding_tree_block_size_minus3);
    goto default_setting;
  }
  if (block_size.bits.log2_min_luma_coding_block_size_minus3 >
      block_size.bits.log2_min_coding_tree_block_size_minus3) {
    GST_WARNING_OBJECT (self, "Invalid log2_min_luma_coding_block_size_minus3:"
        " %d, bigger than log2_min_coding_tree_block_size_minus3: %d",
        block_size.bits.log2_min_luma_coding_block_size_minus3,
        block_size.bits.log2_min_coding_tree_block_size_minus3);
    block_size.bits.log2_min_coding_tree_block_size_minus3 =
        block_size.bits.log2_min_luma_coding_block_size_minus3;
  }

  self->ctu_size =
      1 << (block_size.bits.log2_max_coding_tree_block_size_minus3 + 3);
  self->min_coding_block_size =
      1 << (block_size.bits.log2_min_luma_coding_block_size_minus3 + 3);
  self->features.log2_min_luma_coding_block_size_minus3 =
      block_size.bits.log2_min_luma_coding_block_size_minus3;
  self->features.log2_diff_max_min_luma_coding_block_size =
      block_size.bits.log2_max_coding_tree_block_size_minus3 -
      block_size.bits.log2_min_luma_coding_block_size_minus3;

  if (block_size.bits.log2_min_luma_transform_block_size_minus2 >
      block_size.bits.log2_max_luma_transform_block_size_minus2) {
    GST_WARNING_OBJECT (self, "Invalid"
        " log2_min_luma_transform_block_size_minus2: %d, bigger"
        " than log2_max_luma_transform_block_size_minus2: %d",
        block_size.bits.log2_min_luma_transform_block_size_minus2,
        block_size.bits.log2_max_luma_transform_block_size_minus2);
    goto default_setting;
  }
  self->features.log2_min_transform_block_size_minus2 =
      block_size.bits.log2_min_luma_transform_block_size_minus2;
  self->features.log2_diff_max_min_transform_block_size =
      block_size.bits.log2_max_luma_transform_block_size_minus2 -
      block_size.bits.log2_min_luma_transform_block_size_minus2;

  self->features.max_transform_hierarchy_depth_inter =
      block_size.bits.max_max_transform_hierarchy_depth_inter;
  self->features.max_transform_hierarchy_depth_intra =
      block_size.bits.max_max_transform_hierarchy_depth_intra;

  /* For PCM setting later. */
  self->features.log2_min_pcm_luma_coding_block_size_minus3 =
      block_size.bits.log2_min_pcm_coding_block_size_minus3;
  self->features.log2_max_pcm_luma_coding_block_size_minus3 =
      block_size.bits.log2_max_pcm_coding_block_size_minus3;

  if (self->features.log2_max_pcm_luma_coding_block_size_minus3 -
      self->features.log2_min_pcm_luma_coding_block_size_minus3 >
      self->features.log2_diff_max_min_luma_coding_block_size) {
    GST_WARNING_OBJECT (self, "Invalid"
        " log2_diff_max_min_pcm_luma_coding_block_size: %d",
        self->features.log2_max_pcm_luma_coding_block_size_minus3
        - self->features.log2_min_pcm_luma_coding_block_size_minus3);
    self->features.log2_max_pcm_luma_coding_block_size_minus3 = 0;
    self->features.log2_min_pcm_luma_coding_block_size_minus3 = 0;
  }

  goto done;

default_setting:
#endif

  GST_DEBUG_OBJECT (self, "Apply default setting for coding block");

  /* choose some conservative value */
  self->ctu_size = 32;
  self->min_coding_block_size = 8;
  self->features.log2_min_luma_coding_block_size_minus3 = 0;
  self->features.log2_diff_max_min_luma_coding_block_size = 2;

  self->features.log2_min_transform_block_size_minus2 = 0;
  self->features.log2_diff_max_min_transform_block_size = 3;
  self->features.max_transform_hierarchy_depth_inter = 2;
  self->features.max_transform_hierarchy_depth_intra = 2;
  self->features.pcm_sample_bit_depth_luma_minus1 = 0;
  self->features.pcm_sample_bit_depth_chroma_minus1 = 0;
  /* Default PCM is disabled. */
  self->features.log2_min_pcm_luma_coding_block_size_minus3 = 0;
  self->features.log2_max_pcm_luma_coding_block_size_minus3 = 0;

#if VA_CHECK_VERSION(1, 13, 0)
done:
#endif
  GST_DEBUG_OBJECT (self, "Set coding block size to: "
      "log2_min_luma_coding_block_size_minus3: %d, "
      "log2_diff_max_min_luma_coding_block_size: %d, "
      "log2_min_transform_block_size_minus2: %d, "
      "log2_diff_max_min_transform_block_size: %d, "
      "max_transform_hierarchy_depth_inter: %d, "
      "max_transform_hierarchy_depth_intra: %d",
      self->features.log2_min_luma_coding_block_size_minus3,
      self->features.log2_diff_max_min_luma_coding_block_size,
      self->features.log2_min_transform_block_size_minus2,
      self->features.log2_diff_max_min_transform_block_size,
      self->features.max_transform_hierarchy_depth_inter,
      self->features.max_transform_hierarchy_depth_intra);
}

static gboolean
gst_va_h265_enc_reconfig (GstVaBaseEnc * base)
{
  GstVaBaseEncClass *klass = GST_VA_BASE_ENC_GET_CLASS (base);
  GstVideoEncoder *venc = GST_VIDEO_ENCODER (base);
  GstVaH265Enc *self = GST_VA_H265_ENC (base);
  GstCaps *out_caps, *reconf_caps = NULL;;
  GstVideoCodecState *output_state = NULL;
  GstVideoFormat format, reconf_format = GST_VIDEO_FORMAT_UNKNOWN;
  VAProfile profile = VAProfileNone;
  gboolean do_renegotiation = TRUE, do_reopen, need_negotiation;
  guint max_ref_frames, max_surfaces = 0, rt_format = 0, codedbuf_size;
  gint width, height;
  guint alignment;

  width = GST_VIDEO_INFO_WIDTH (&base->in_info);
  height = GST_VIDEO_INFO_HEIGHT (&base->in_info);
  format = GST_VIDEO_INFO_FORMAT (&base->in_info);
  codedbuf_size = base->codedbuf_size;

  need_negotiation =
      !gst_va_encoder_get_reconstruct_pool_config (base->encoder, &reconf_caps,
      &max_surfaces);
  if (!need_negotiation && reconf_caps) {
    GstVideoInfo vi;
    if (!gst_video_info_from_caps (&vi, reconf_caps))
      return FALSE;
    reconf_format = GST_VIDEO_INFO_FORMAT (&vi);
  }

  if (!_h265_decide_profile (self, &profile, &rt_format))
    return FALSE;

  /* first check */
  do_reopen = !(base->profile == profile && base->rt_format == rt_format
      && format == reconf_format && width == base->width
      && height == base->height && self->prop.rc_ctrl == self->rc.rc_ctrl_mode);

  if (do_reopen && gst_va_encoder_is_open (base->encoder))
    gst_va_encoder_close (base->encoder);

  gst_va_base_enc_reset_state (base);

  base->profile = profile;
  base->rt_format = rt_format;
  base->width = width;
  base->height = height;

  alignment = gst_va_encoder_get_surface_alignment (base->display,
      profile, klass->entrypoint);
  if (alignment) {
    self->luma_width = GST_ROUND_UP_N (base->width, 1 << (alignment & 0xf));
    self->luma_height =
        GST_ROUND_UP_N (base->height, 1 << ((alignment & 0xf0) >> 4));
  } else {
    self->luma_width = GST_ROUND_UP_16 (base->width);
    self->luma_height = GST_ROUND_UP_16 (base->height);
  }

  /* Frame Cropping */
  if (self->luma_width != base->width || self->luma_height != base->height) {
    /* 6.1, Table 6-1 */
    static const guint SubWidthC[] = { 1, 2, 2, 1 };
    static const guint SubHeightC[] = { 1, 2, 1, 1 };
    guint index = _get_chroma_format_idc (gst_va_chroma_from_video_format
        (GST_VIDEO_INFO_FORMAT (&base->in_info)));

    self->conformance_window_flag = 1;
    self->conf_win_left_offset = 0;
    self->conf_win_right_offset =
        (self->luma_width - base->width) / SubWidthC[index];
    self->conf_win_top_offset = 0;
    self->conf_win_bottom_offset =
        (self->luma_height - base->height) / SubHeightC[index];
  }

  _h265_set_coding_block_size (self);

  self->ctu_width = (self->luma_width + self->ctu_size - 1) / self->ctu_size;
  self->ctu_height = (self->luma_height + self->ctu_size - 1) / self->ctu_size;
  if (self->ctu_width == 0 || self->ctu_height == 0)
    return FALSE;

  self->bits_depth_luma_minus8 =
      GST_VIDEO_FORMAT_INFO_DEPTH (base->in_info.finfo, 0);
  self->bits_depth_luma_minus8 -= 8;

  if (GST_VIDEO_FORMAT_INFO_N_COMPONENTS (base->in_info.finfo)) {
    self->bits_depth_chroma_minus8 =
        GST_VIDEO_FORMAT_INFO_DEPTH (base->in_info.finfo, 1);
    if (self->bits_depth_chroma_minus8 <
        GST_VIDEO_FORMAT_INFO_DEPTH (base->in_info.finfo, 2))
      self->bits_depth_chroma_minus8 =
          GST_VIDEO_FORMAT_INFO_DEPTH (base->in_info.finfo, 2);

    self->bits_depth_chroma_minus8 -= 8;
  } else {
    self->bits_depth_chroma_minus8 = 0;
  }

  /* Frame rate is needed for rate control and PTS setting. */
  if (GST_VIDEO_INFO_FPS_N (&base->in_info) == 0
      || GST_VIDEO_INFO_FPS_D (&base->in_info) == 0) {
    GST_INFO_OBJECT (self, "Unknown framerate, just set to 30 fps");
    GST_VIDEO_INFO_FPS_N (&base->in_info) = 30;
    GST_VIDEO_INFO_FPS_D (&base->in_info) = 1;
  }
  base->frame_duration = gst_util_uint64_scale (GST_SECOND,
      GST_VIDEO_INFO_FPS_D (&base->in_info),
      GST_VIDEO_INFO_FPS_N (&base->in_info));

  GST_DEBUG_OBJECT (self, "resolution:%dx%d, CTU size: %dx%d,"
      " frame duration is %" GST_TIME_FORMAT,
      base->width, base->height, self->ctu_width, self->ctu_height,
      GST_TIME_ARGS (base->frame_duration));

  if (!_h265_ensure_rate_control (self))
    return FALSE;

  if (!_h265_calculate_tier_level (self))
    return FALSE;

  if (!_h265_generate_gop_structure (self))
    return FALSE;

  _h265_setup_encoding_features (self);

  _h265_calculate_coded_size (self);

  if (!_h265_setup_slice_and_tile_partition (self))
    return FALSE;

  if (!_h265_init_packed_headers (self))
    return FALSE;

  self->aud = self->aud && self->packed_headers & VA_ENC_PACKED_HEADER_RAW_DATA;
  update_property_bool (base, &self->prop.aud, self->aud, PROP_AUD);

  max_ref_frames = self->gop.num_ref_frames + 3 /* scratch frames */ ;

  /* second check after calculations */
  do_reopen |=
      !(max_ref_frames == max_surfaces && codedbuf_size == base->codedbuf_size);
  if (do_reopen && gst_va_encoder_is_open (base->encoder))
    gst_va_encoder_close (base->encoder);

  if (!gst_va_encoder_is_open (base->encoder)
      && !gst_va_encoder_open (base->encoder, base->profile,
          format, base->rt_format, self->luma_width, self->luma_height,
          base->codedbuf_size, max_ref_frames, self->rc.rc_ctrl_mode,
          self->packed_headers)) {
    GST_ERROR_OBJECT (self, "Failed to open the VA encoder.");
    return FALSE;
  }

  /* Add some tags */
  gst_va_base_enc_add_codec_tag (base, "H265");

  out_caps = gst_va_profile_caps (base->profile, klass->entrypoint);
  g_assert (out_caps);
  out_caps = gst_caps_fixate (out_caps);

  if (self->level_str)
    gst_caps_set_simple (out_caps, "level", G_TYPE_STRING, self->level_str,
        NULL);

  gst_caps_set_simple (out_caps, "width", G_TYPE_INT, base->width,
      "height", G_TYPE_INT, base->height, "alignment", G_TYPE_STRING, "au",
      "stream-format", G_TYPE_STRING, "byte-stream", NULL);

  if (!need_negotiation) {
    output_state = gst_video_encoder_get_output_state (venc);
    do_renegotiation = TRUE;

    if (output_state) {
      do_renegotiation = !gst_caps_is_subset (output_state->caps, out_caps);
      gst_video_codec_state_unref (output_state);
    }

    if (!do_renegotiation) {
      gst_caps_unref (out_caps);
      return TRUE;
    }
  }

  GST_DEBUG_OBJECT (self, "output caps is %" GST_PTR_FORMAT, out_caps);

  output_state =
      gst_video_encoder_set_output_state (venc, out_caps, base->input_state);
  gst_video_codec_state_unref (output_state);

  if (!gst_video_encoder_negotiate (venc)) {
    GST_ERROR_OBJECT (self, "Failed to negotiate with the downstream");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_va_h265_enc_flush (GstVideoEncoder * venc)
{
  GstVaH265Enc *self = GST_VA_H265_ENC (venc);

  /* begin from an IDR after flush. */
  self->gop.cur_frame_index = 0;
  self->gop.last_keyframe = NULL;

  return GST_VIDEO_ENCODER_CLASS (parent_class)->flush (venc);
}

static gboolean
gst_va_h265_enc_start (GstVideoEncoder * venc)
{
  /* Set the minimum pts to some huge value (1000 hours). This keeps
   * the dts at the start of the stream from needing to be negative. */
  gst_video_encoder_set_min_pts (venc, GST_SECOND * 60 * 60 * 1000);

  return GST_VIDEO_ENCODER_CLASS (parent_class)->start (venc);
}

static gboolean
gst_va_h265_enc_new_frame (GstVaBaseEnc * base, GstVideoCodecFrame * frame)
{
  GstVaH265Enc *self = GST_VA_H265_ENC (base);
  GstVaH265EncFrame *frame_in;

  frame_in = gst_va_h265_enc_frame_new ();
  gst_video_codec_frame_set_user_data (frame, frame_in,
      gst_va_h265_enc_frame_free);

  gst_va_base_enc_push_dts (base, frame, self->gop.num_reorder_frames);

  return TRUE;
}

static gboolean
gst_va_h265_enc_prepare_output (GstVaBaseEnc * base,
    GstVideoCodecFrame * frame, gboolean * complete)
{
  GstVaH265EncFrame *frame_enc;
  GstBuffer *buf;

  frame_enc = _enc_frame (frame);

  frame->dts = gst_va_base_enc_pop_dts (base);
  if (!GST_CLOCK_TIME_IS_VALID (frame->dts)) {
    GST_DEBUG_OBJECT (base, "Pop invalid DTS.");
  } else if (GST_CLOCK_TIME_IS_VALID (frame->pts) && frame->dts > frame->pts) {
    GST_WARNING_OBJECT (base, "Pop DTS: %" GST_TIME_FORMAT " > PTS: %"
        GST_TIME_FORMAT, GST_TIME_ARGS (frame->dts),
        GST_TIME_ARGS (frame->pts));
    frame->dts = frame->pts;
  }

  buf = gst_va_base_enc_create_output_buffer (base,
      frame_enc->picture, NULL, 0);
  if (!buf) {
    GST_ERROR_OBJECT (base, "Failed to create output buffer");
    return FALSE;
  }

  GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_MARKER);
  if (frame_enc->poc == 0) {
    GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);
    GST_BUFFER_FLAG_UNSET (buf, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_HEADER);
  } else {
    GST_VIDEO_CODEC_FRAME_UNSET_SYNC_POINT (frame);
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT);
  }

  gst_buffer_replace (&frame->output_buffer, buf);
  gst_clear_buffer (&buf);

  *complete = TRUE;
  return TRUE;
}

/* *INDENT-OFF* */
static const gchar *sink_caps_str =
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_VA,
        "{ NV12 }") " ;"
    GST_VIDEO_CAPS_MAKE ("{ NV12 }");
/* *INDENT-ON* */

static const gchar *src_caps_str = "video/x-h265";

static gpointer
_register_debug_category (gpointer data)
{
  GST_DEBUG_CATEGORY_INIT (gst_va_h265enc_debug, "vah265enc", 0,
      "VA h265 encoder");

  return NULL;
}

static void
gst_va_h265_enc_init (GTypeInstance * instance, gpointer g_class)
{
  GstVaH265Enc *self = GST_VA_H265_ENC (instance);

  /* default values */
  self->prop.key_int_max = 0;
  self->prop.num_bframes = 0;
  self->prop.num_iframes = 0;
  self->prop.num_ref_frames = 3;
  self->prop.b_pyramid = FALSE;
  self->prop.num_slices = 1;
  self->prop.min_qp = 1;
  self->prop.max_qp = 51;
  self->prop.qp_i = 26;
  self->prop.qp_p = 26;
  self->prop.qp_b = 26;
  self->prop.use_trellis = FALSE;
  self->prop.aud = FALSE;
  self->prop.mbbrc = 0;
  self->prop.bitrate = 0;
  self->prop.target_percentage = 66;
  self->prop.target_usage = 4;
  self->prop.cpb_size = 0;
  if (properties[PROP_RATE_CONTROL]) {
    self->prop.rc_ctrl =
        G_PARAM_SPEC_ENUM (properties[PROP_RATE_CONTROL])->default_value;
  } else {
    self->prop.rc_ctrl = VA_RC_NONE;
  }
}

static void
gst_va_h265_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVaH265Enc *const self = GST_VA_H265_ENC (object);
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  gboolean already_effect = FALSE;

  GST_OBJECT_LOCK (self);

  switch (prop_id) {
    case PROP_KEY_INT_MAX:
      self->prop.key_int_max = g_value_get_uint (value);
      break;
    case PROP_BFRAMES:
      self->prop.num_bframes = g_value_get_uint (value);
      break;
    case PROP_IFRAMES:
      self->prop.num_iframes = g_value_get_uint (value);
      break;
    case PROP_NUM_REF_FRAMES:
      self->prop.num_ref_frames = g_value_get_uint (value);
      break;
    case PROP_B_PYRAMID:
      self->prop.b_pyramid = g_value_get_boolean (value);
      break;
    case PROP_NUM_SLICES:
      self->prop.num_slices = g_value_get_uint (value);
      break;
    case PROP_MIN_QP:
      self->prop.min_qp = g_value_get_uint (value);
      break;
    case PROP_MAX_QP:
      self->prop.max_qp = g_value_get_uint (value);
      break;
    case PROP_QP_I:
      self->prop.qp_i = g_value_get_uint (value);
      g_atomic_int_set (&GST_VA_BASE_ENC (self)->reconf, TRUE);
      already_effect = TRUE;
      break;
    case PROP_QP_P:
      self->prop.qp_p = g_value_get_uint (value);
      g_atomic_int_set (&GST_VA_BASE_ENC (self)->reconf, TRUE);
      already_effect = TRUE;
      break;
    case PROP_QP_B:
      self->prop.qp_b = g_value_get_uint (value);
      g_atomic_int_set (&GST_VA_BASE_ENC (self)->reconf, TRUE);
      already_effect = TRUE;
      break;
    case PROP_TRELLIS:
      self->prop.use_trellis = g_value_get_boolean (value);
      break;
    case PROP_AUD:
      self->prop.aud = g_value_get_boolean (value);
      break;
    case PROP_MBBRC:{
      /* Macroblock-level rate control.
       * 0: use default,
       * 1: always enable,
       * 2: always disable,
       * other: reserved. */
      switch (g_value_get_enum (value)) {
        case GST_VA_FEATURE_DISABLED:
          self->prop.mbbrc = 2;
          break;
        case GST_VA_FEATURE_ENABLED:
          self->prop.mbbrc = 1;
          break;
        case GST_VA_FEATURE_AUTO:
          self->prop.mbbrc = 0;
          break;
      }
      break;
    }
    case PROP_BITRATE:
      self->prop.bitrate = g_value_get_uint (value);
      g_atomic_int_set (&GST_VA_BASE_ENC (self)->reconf, TRUE);
      already_effect = TRUE;
      break;
    case PROP_TARGET_PERCENTAGE:
      self->prop.target_percentage = g_value_get_uint (value);
      g_atomic_int_set (&GST_VA_BASE_ENC (self)->reconf, TRUE);
      already_effect = TRUE;
      break;
    case PROP_TARGET_USAGE:
      self->prop.target_usage = g_value_get_uint (value);
      g_atomic_int_set (&GST_VA_BASE_ENC (self)->reconf, TRUE);
      already_effect = TRUE;
      break;
    case PROP_NUM_TILE_COLS:
      self->prop.num_tile_cols = g_value_get_uint (value);
      break;
    case PROP_NUM_TILE_ROWS:
      self->prop.num_tile_rows = g_value_get_uint (value);
      break;
    case PROP_RATE_CONTROL:
      self->prop.rc_ctrl = g_value_get_enum (value);
      g_atomic_int_set (&GST_VA_BASE_ENC (self)->reconf, TRUE);
      already_effect = TRUE;
      break;
    case PROP_CPB_SIZE:
      self->prop.cpb_size = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }

  GST_OBJECT_UNLOCK (self);

#ifndef GST_DISABLE_GST_DEBUG
  if (!already_effect &&
      base->encoder && gst_va_encoder_is_open (base->encoder)) {
    GST_WARNING_OBJECT (self, "Property `%s` change ignored while processing.",
        pspec->name);
  }
#endif
}

static void
gst_va_h265_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVaH265Enc *const self = GST_VA_H265_ENC (object);

  GST_OBJECT_LOCK (self);

  switch (prop_id) {
    case PROP_KEY_INT_MAX:
      g_value_set_uint (value, self->prop.key_int_max);
      break;
    case PROP_BFRAMES:
      g_value_set_uint (value, self->prop.num_bframes);
      break;
    case PROP_IFRAMES:
      g_value_set_uint (value, self->prop.num_iframes);
      break;
    case PROP_NUM_REF_FRAMES:
      g_value_set_uint (value, self->prop.num_ref_frames);
      break;
    case PROP_B_PYRAMID:
      g_value_set_boolean (value, self->prop.b_pyramid);
      break;
    case PROP_NUM_SLICES:
      g_value_set_uint (value, self->prop.num_slices);
      break;
    case PROP_MIN_QP:
      g_value_set_uint (value, self->prop.min_qp);
      break;
    case PROP_MAX_QP:
      g_value_set_uint (value, self->prop.max_qp);
      break;
    case PROP_QP_I:
      g_value_set_uint (value, self->prop.qp_i);
      break;
    case PROP_QP_P:
      g_value_set_uint (value, self->prop.qp_p);
      break;
    case PROP_QP_B:
      g_value_set_uint (value, self->prop.qp_b);
      break;
    case PROP_TRELLIS:
      g_value_set_boolean (value, self->prop.use_trellis);
      break;
    case PROP_AUD:
      g_value_set_boolean (value, self->prop.aud);
      break;
    case PROP_MBBRC:{
      GstVaFeature mbbrc = GST_VA_FEATURE_AUTO;
      /* Macroblock-level rate control.
       * 0: use default,
       * 1: always enable,
       * 2: always disable,
       * other: reserved. */
      switch (self->prop.mbbrc) {
        case 2:
          mbbrc = GST_VA_FEATURE_DISABLED;
          break;
        case 1:
          mbbrc = GST_VA_FEATURE_ENABLED;
          break;
        case 0:
          mbbrc = GST_VA_FEATURE_AUTO;
          break;
        default:
          g_assert_not_reached ();
      }

      g_value_set_enum (value, mbbrc);
      break;
    }
    case PROP_BITRATE:
      g_value_set_uint (value, self->prop.bitrate);
      break;
    case PROP_TARGET_PERCENTAGE:
      g_value_set_uint (value, self->prop.target_percentage);
      break;
    case PROP_TARGET_USAGE:
      g_value_set_uint (value, self->prop.target_usage);
      break;
    case PROP_NUM_TILE_COLS:
      g_value_set_uint (value, self->prop.num_tile_cols);
      break;
    case PROP_NUM_TILE_ROWS:
      g_value_set_uint (value, self->prop.num_tile_rows);
      break;
    case PROP_RATE_CONTROL:
      g_value_set_enum (value, self->prop.rc_ctrl);
      break;
    case PROP_CPB_SIZE:
      g_value_set_uint (value, self->prop.cpb_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }

  GST_OBJECT_UNLOCK (self);
}

static void
gst_va_h265_enc_dispose (GObject * object)
{
  GstVaH265Enc *self = GST_VA_H265_ENC (object);

  g_clear_pointer (&self->partition.slice_segment_address, g_free);
  g_clear_pointer (&self->partition.num_ctu_in_slice, g_free);
  g_clear_pointer (&self->partition.tile_ctu_cols, g_free);
  g_clear_pointer (&self->partition.tile_ctu_rows, g_free);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_va_h265_enc_class_init (gpointer g_klass, gpointer class_data)
{
  GstCaps *src_doc_caps, *sink_doc_caps;
  GstPadTemplate *sink_pad_templ, *src_pad_templ;
  GObjectClass *object_class = G_OBJECT_CLASS (g_klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_klass);
  GstVideoEncoderClass *venc_class = GST_VIDEO_ENCODER_CLASS (g_klass);
  GstVaBaseEncClass *va_enc_class = GST_VA_BASE_ENC_CLASS (g_klass);
  GstVaH265EncClass *vah265enc_class = GST_VA_H265_ENC_CLASS (g_klass);
  GstVaDisplay *display;
  GstVaEncoder *encoder;
  struct CData *cdata = class_data;
  gchar *long_name;
  const gchar *name, *desc;
  gint n_props = N_PROPERTIES;
  GParamFlags param_flags =
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT;

  if (cdata->entrypoint == VAEntrypointEncSlice) {
    desc = "VA-API based H.265 video encoder";
    name = "VA-API H.265 Encoder";
  } else {
    desc = "VA-API based H.265 low power video encoder";
    name = "VA-API H.265 Low Power Encoder";
  }

  if (cdata->description)
    long_name = g_strdup_printf ("%s in %s", name, cdata->description);
  else
    long_name = g_strdup (name);

  gst_element_class_set_metadata (element_class, long_name,
      "Codec/Encoder/Video/Hardware", desc, "He Junyan <junyan.he@intel.com>");

  sink_doc_caps = gst_caps_from_string (sink_caps_str);
  src_doc_caps = gst_caps_from_string (src_caps_str);

  parent_class = g_type_class_peek_parent (g_klass);

  va_enc_class->codec = HEVC;
  va_enc_class->entrypoint = cdata->entrypoint;
  va_enc_class->render_device_path = g_strdup (cdata->render_device_path);

  sink_pad_templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      cdata->sink_caps);
  gst_element_class_add_pad_template (element_class, sink_pad_templ);

  gst_pad_template_set_documentation_caps (sink_pad_templ, sink_doc_caps);
  gst_caps_unref (sink_doc_caps);

  src_pad_templ = gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
      cdata->src_caps);
  gst_element_class_add_pad_template (element_class, src_pad_templ);

  gst_pad_template_set_documentation_caps (src_pad_templ, src_doc_caps);
  gst_caps_unref (src_doc_caps);

  object_class->set_property = gst_va_h265_enc_set_property;
  object_class->get_property = gst_va_h265_enc_get_property;
  object_class->dispose = gst_va_h265_enc_dispose;

  venc_class->flush = GST_DEBUG_FUNCPTR (gst_va_h265_enc_flush);
  venc_class->start = GST_DEBUG_FUNCPTR (gst_va_h265_enc_start);

  va_enc_class->reset_state = GST_DEBUG_FUNCPTR (gst_va_h265_enc_reset_state);
  va_enc_class->reconfig = GST_DEBUG_FUNCPTR (gst_va_h265_enc_reconfig);
  va_enc_class->new_frame = GST_DEBUG_FUNCPTR (gst_va_h265_enc_new_frame);
  va_enc_class->reorder_frame =
      GST_DEBUG_FUNCPTR (gst_va_h265_enc_reorder_frame);
  va_enc_class->encode_frame = GST_DEBUG_FUNCPTR (gst_va_h265_enc_encode_frame);
  va_enc_class->prepare_output =
      GST_DEBUG_FUNCPTR (gst_va_h265_enc_prepare_output);

  {
    display = gst_va_display_platform_new (va_enc_class->render_device_path);
    encoder = gst_va_encoder_new (display, va_enc_class->codec,
        va_enc_class->entrypoint);
    if (gst_va_encoder_get_rate_control_enum (encoder,
            vah265enc_class->rate_control)) {
      gchar *basename = g_path_get_basename (va_enc_class->render_device_path);
      g_snprintf (vah265enc_class->rate_control_type_name,
          G_N_ELEMENTS (vah265enc_class->rate_control_type_name) - 1,
          "GstVaEncoderRateControl_%" GST_FOURCC_FORMAT "%s_%s",
          GST_FOURCC_ARGS (va_enc_class->codec),
          (va_enc_class->entrypoint == VAEntrypointEncSliceLP) ? "_LP" : "",
          basename);
      vah265enc_class->rate_control_type =
          g_enum_register_static (vah265enc_class->rate_control_type_name,
          vah265enc_class->rate_control);
      gst_type_mark_as_plugin_api (vah265enc_class->rate_control_type, 0);
      g_free (basename);
    }
    gst_object_unref (encoder);
    gst_object_unref (display);
  }

  g_free (long_name);
  g_free (cdata->description);
  g_free (cdata->render_device_path);
  gst_caps_unref (cdata->src_caps);
  gst_caps_unref (cdata->sink_caps);
  g_free (cdata);

  /**
   * GstVaH265Enc:key-int-max:
   *
   * The maximal distance between two keyframes.
   */
  properties[PROP_KEY_INT_MAX] = g_param_spec_uint ("key-int-max",
      "Key frame maximal interval",
      "The maximal distance between two keyframes. It decides the size of GOP"
      " (0: auto-calculate)", 0, MAX_GOP_SIZE, 0, param_flags);

  /**
   * GstVaH265Enc:b-frames:
   *
   * Number of B-frames between two reference frames.
   */
  properties[PROP_BFRAMES] = g_param_spec_uint ("b-frames", "B Frames",
      "Number of B frames between I and P reference frames", 0, 31, 0,
      param_flags);

  /**
   * GstVaH265Enc:i-frames:
   *
   * Force the number of i-frames insertion within one GOP.
   */
  properties[PROP_IFRAMES] = g_param_spec_uint ("i-frames", "I Frames",
      "Force the number of I frames insertion within one GOP, not including the "
      "first IDR frame", 0, 1023, 0, param_flags);

  /* The VA only define 15 refs */
  /**
   * GstVaH265Enc:ref-frames:
   *
   * The number of reference frames.
   */
  properties[PROP_NUM_REF_FRAMES] = g_param_spec_uint ("ref-frames",
      "Number of Reference Frames",
      "Number of reference frames, including both the forward and the backward",
      0, 15, 3, param_flags);

  /**
   * GstVaH265Enc:b-pyramid:
   *
   * Enable the b-pyramid reference structure in GOP.
   */
  properties[PROP_B_PYRAMID] = g_param_spec_boolean ("b-pyramid", "b pyramid",
      "Enable the b-pyramid reference structure in the GOP", FALSE,
      param_flags);

  /**
   * GstVaH265Enc:num-slices:
   *
   * The number of slices per frame.
   */
  properties[PROP_NUM_SLICES] = g_param_spec_uint ("num-slices",
      "Number of Slices", "Number of slices per frame", 1, 200, 1, param_flags);

  /**
   * GstVaH265Enc:max-qp:
   *
   * The maximum quantizer value.
   */
  properties[PROP_MAX_QP] = g_param_spec_uint ("max-qp", "Maximum QP",
      "Maximum quantizer value for each frame", 0, 51, 51, param_flags);

  /**
   * GstVaH265Enc:min-qp:
   *
   * The minimum quantizer value.
   */
  properties[PROP_MIN_QP] = g_param_spec_uint ("min-qp", "Minimum QP",
      "Minimum quantizer value for each frame", 0, 51, 1, param_flags);

  /**
   * GstVaH265Enc:qpi:
   *
   * The quantizer value for I frame. In CQP mode, it specifies the QP of
   * I frame, in other mode, it specifies the init QP of all frames.
   */
  properties[PROP_QP_I] = g_param_spec_uint ("qpi", "I Frame QP",
      "The quantizer value for I frame. In CQP mode, it specifies the QP of I "
      "frame, in other mode, it specifies the init QP of all frames", 0, 51, 26,
      param_flags | GST_PARAM_MUTABLE_PLAYING);

  /**
   * GstVaH265Enc:qpp:
   *
   * The quantizer value for P frame. This is available only in CQP mode.
   */
  properties[PROP_QP_P] = g_param_spec_uint ("qpp",
      "The quantizer value for P frame",
      "The quantizer value for P frame. This is available only in CQP mode",
      0, 51, 26, param_flags | GST_PARAM_MUTABLE_PLAYING);

  /**
   * GstVaH265Enc:qpb:
   *
   * The quantizer value for B frame. This is available only in CQP mode.
   */
  properties[PROP_QP_B] = g_param_spec_uint ("qpb",
      "The quantizer value for B frame",
      "The quantizer value for B frame. This is available only in CQP mode",
      0, 51, 26, param_flags | GST_PARAM_MUTABLE_PLAYING);

  /**
   * GstVaH265Enc:trellis:
   *
   * It enable the trellis quantization method.
   * Trellis is an improved quantization algorithm.
   */
  properties[PROP_TRELLIS] = g_param_spec_boolean ("trellis", "Enable trellis",
      "Enable the trellis quantization method", FALSE, param_flags);

  /**
   * GstVaH265Enc:aud:
   *
   * Insert the AU (Access Unit) delimeter for each frame.
   */
  properties[PROP_AUD] = g_param_spec_boolean ("aud", "Insert AUD",
      "Insert AU (Access Unit) delimeter for each frame", FALSE, param_flags);

  /**
   * GstVaH265Enc:mbbrc:
   *
   * Macroblock level bitrate control.
   * This is not compatible with Constant QP rate control.
   */
  properties[PROP_MBBRC] = g_param_spec_enum ("mbbrc",
      "Macroblock level Bitrate Control",
      "Macroblock level Bitrate Control. It is not compatible with CQP",
      GST_TYPE_VA_FEATURE, GST_VA_FEATURE_AUTO, param_flags);

  /**
   * GstVaH265Enc:bitrate:
   *
   * The desired target bitrate, expressed in kbps.
   * This is not available in CQP mode.
   *
   * CBR: This applies equally to the minimum, maximum and target bitrate.
   * VBR: This applies to the target bitrate. The driver will use the
   * "target-percentage" together to calculate the minimum and maximum bitrate.
   * VCM: This applies to the target bitrate. The minimum and maximum bitrate
   * are not needed.
   */
  properties[PROP_BITRATE] = g_param_spec_uint ("bitrate", "Bitrate (kbps)",
      "The desired bitrate expressed in kbps (0: auto-calculate)",
      0, 2000 * 1024, 0, param_flags | GST_PARAM_MUTABLE_PLAYING);

  /**
   * GstVaH265Enc:target-percentage:
   *
   * The target percentage of the max bitrate, and expressed in uint,
   * equal to "target percentage"*100.
   * "target percentage" = "target bitrate" * 100 / "max bitrate"
   * This is available only when rate-control is VBR.
   * The driver uses it to calculate the minimum and maximum bitrate.
   */
  properties[PROP_TARGET_PERCENTAGE] = g_param_spec_uint ("target-percentage",
      "target bitrate percentage",
      "The percentage for 'target bitrate'/'maximum bitrate' (Only in VBR)",
      50, 100, 66, param_flags | GST_PARAM_MUTABLE_PLAYING);

  /**
   * GstVaH265Enc:target-usage:
   *
   * The target usage of the encoder. It controls and balances the encoding
   * speed and the encoding quality. The lower value has better quality but
   * slower speed, the higher value has faster speed but lower quality.
   */
  properties[PROP_TARGET_USAGE] = g_param_spec_uint ("target-usage",
      "target usage",
      "The target usage to control and balance the encoding speed/quality",
      1, 7, 4, param_flags | GST_PARAM_MUTABLE_PLAYING);

  /**
   * GstVaH265Enc:cpb-size:
   *
   * The desired max CPB size in Kb (0: auto-calculate).
   */
  properties[PROP_CPB_SIZE] = g_param_spec_uint ("cpb-size",
      "max CPB size in Kb",
      "The desired max CPB size in Kb (0: auto-calculate)", 0, 2000 * 1024, 0,
      param_flags);

  /**
   * GstVaH265Enc:num-tile-cols:
   *
   * The number of tile columns when tile encoding is enabled.
   */
  properties[PROP_NUM_TILE_COLS] = g_param_spec_uint ("num-tile-cols",
      "number of tile columns", "The number of columns for tile encoding",
      1, MAX_COL_TILES, 1, param_flags);

  /**
   * GstVaH265Enc:num-tile-rows:
   *
   * The number of tile rows when tile encoding is enabled.
   */
  properties[PROP_NUM_TILE_ROWS] = g_param_spec_uint ("num-tile-rows",
      "number of tile rows", "The number of rows for tile encoding",
      1, MAX_ROW_TILES, 1, param_flags);

  if (vah265enc_class->rate_control_type > 0) {
    properties[PROP_RATE_CONTROL] = g_param_spec_enum ("rate-control",
        "rate control mode", "The desired rate control mode for the encoder",
        vah265enc_class->rate_control_type,
        vah265enc_class->rate_control[0].value,
        GST_PARAM_CONDITIONALLY_AVAILABLE | GST_PARAM_MUTABLE_PLAYING
        | param_flags);
  } else {
    n_props--;
    properties[PROP_RATE_CONTROL] = NULL;
  }

  g_object_class_install_properties (object_class, n_props, properties);

  /**
   * GstVaFeature:
   * @GST_VA_FEATURE_DISABLED: The feature is disabled.
   * @GST_VA_FEATURE_ENABLED: The feature is enabled.
   * @GST_VA_FEATURE_AUTO: The feature is enabled automatically.
   *
   * Since: 1.22
   */
  gst_type_mark_as_plugin_api (GST_TYPE_VA_FEATURE, 0);
}

static GstCaps *
_complete_src_caps (GstCaps * srccaps)
{
  GstCaps *caps = gst_caps_copy (srccaps);
  GValue val = G_VALUE_INIT;

  g_value_init (&val, G_TYPE_STRING);
  g_value_set_string (&val, "au");
  gst_caps_set_value (caps, "alignment", &val);
  g_value_unset (&val);

  g_value_init (&val, G_TYPE_STRING);
  g_value_set_string (&val, "byte-stream");
  gst_caps_set_value (caps, "stream-format", &val);
  g_value_unset (&val);

  return caps;
}

gboolean
gst_va_h265_enc_register (GstPlugin * plugin, GstVaDevice * device,
    GstCaps * sink_caps, GstCaps * src_caps, guint rank,
    VAEntrypoint entrypoint)
{
  static GOnce debug_once = G_ONCE_INIT;
  GType type;
  GTypeInfo type_info = {
    .class_size = sizeof (GstVaH265EncClass),
    .class_init = gst_va_h265_enc_class_init,
    .instance_size = sizeof (GstVaH265Enc),
    .instance_init = gst_va_h265_enc_init,
  };
  struct CData *cdata;
  gboolean ret;
  gchar *type_name, *feature_name;

  g_return_val_if_fail (GST_IS_PLUGIN (plugin), FALSE);
  g_return_val_if_fail (GST_IS_VA_DEVICE (device), FALSE);
  g_return_val_if_fail (GST_IS_CAPS (sink_caps), FALSE);
  g_return_val_if_fail (GST_IS_CAPS (src_caps), FALSE);
  g_return_val_if_fail (entrypoint == VAEntrypointEncSlice ||
      entrypoint == VAEntrypointEncSliceLP, FALSE);

  cdata = g_new (struct CData, 1);
  cdata->entrypoint = entrypoint;
  cdata->description = NULL;
  cdata->render_device_path = g_strdup (device->render_device_path);
  cdata->sink_caps = gst_caps_ref (sink_caps);
  cdata->src_caps = _complete_src_caps (src_caps);

  /* class data will be leaked if the element never gets instantiated */
  GST_MINI_OBJECT_FLAG_SET (cdata->sink_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (cdata->src_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  type_info.class_data = cdata;

  if (entrypoint == VAEntrypointEncSlice) {
    gst_va_create_feature_name (device, "GstVaH265Enc", "GstVa%sH265Enc",
        &type_name, "vah265enc", "va%sh265enc", &feature_name,
        &cdata->description, &rank);
  } else {
    gst_va_create_feature_name (device, "GstVaH265LPEnc", "GstVa%sH265LPEnc",
        &type_name, "vah265lpenc", "va%sh265lpenc", &feature_name,
        &cdata->description, &rank);
  }

  g_once (&debug_once, _register_debug_category, NULL);
  type = g_type_register_static (GST_TYPE_VA_BASE_ENC,
      type_name, &type_info, 0);
  ret = gst_element_register (plugin, feature_name, rank, type);

  g_free (type_name);
  g_free (feature_name);

  return ret;
}
