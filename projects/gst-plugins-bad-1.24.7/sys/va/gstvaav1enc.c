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
 * SECTION:element-vaav1enc
 * @title: vaav1enc
 * @short_description: A VA-API based AV1 video encoder
 *
 * vaav1enc encodes raw video VA surfaces into AV1 bitstreams using
 * the installed and chosen [VA-API](https://01.org/linuxmedia/vaapi)
 * driver.
 *
 * The raw video frames in main memory can be imported into VA surfaces.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc num-buffers=60 ! timeoverlay ! vaav1enc ! av1parse ! mp4mux ! filesink location=test.mp4
 * ```
 *
 * Since: 1.22
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvaav1enc.h"

#include <gst/codecparsers/gstav1bitwriter.h>
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

GST_DEBUG_CATEGORY_STATIC (gst_va_av1enc_debug);
#define GST_CAT_DEFAULT gst_va_av1enc_debug

#define GST_VA_AV1_ENC(obj)            ((GstVaAV1Enc *) obj)
#define GST_VA_AV1_ENC_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_FROM_INSTANCE (obj), GstVaAV1EncClass))
#define GST_VA_AV1_ENC_CLASS(klass)    ((GstVaAV1EncClass *) klass)

typedef struct _GstVaAV1Enc GstVaAV1Enc;
typedef struct _GstVaAV1EncClass GstVaAV1EncClass;
typedef struct _GstVaAV1LevelLimits GstVaAV1LevelLimits;
typedef struct _GstVaAV1GFGroup GstVaAV1GFGroup;
typedef struct _GstVaAV1Ref GstVaAV1Ref;
typedef struct _GstVaAV1EncFrame GstVaAV1EncFrame;

enum
{
  PROP_KEYFRAME_INT = 1,
  PROP_GOLDEN_GROUP_SIZE,
  PROP_NUM_REF_FRAMES,
  PROP_HIERARCHICAL_LEVEL,
  PROP_128X128_SUPERBLOCK,
  PROP_MIN_QP,
  PROP_MAX_QP,
  PROP_QP,
  PROP_BITRATE,
  PROP_TARGET_PERCENTAGE,
  PROP_TARGET_USAGE,
  PROP_CPB_SIZE,
  PROP_NUM_TILE_COLS,
  PROP_NUM_TILE_ROWS,
  PROP_TILE_GROUPS,
  PROP_MBBRC,
  PROP_RATE_CONTROL,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

static GstElementClass *parent_class = NULL;

#define DEFAULT_BASE_QINDEX  128

#define MAX_KEY_FRAME_INTERVAL  1024
#define MAX_GF_GROUP_SIZE  32
#define HIGHEST_PYRAMID_LEVELS  6
#define INVALID_PYRAMID_LEVEL  -1

#define FRAME_TYPE_INVALID  -1
#define FRAME_TYPE_REPEAT  0x80
/* The frame is golden frame */
#define FRAME_FLAG_GF  0x01
/* The frame is alt frame */
#define FRAME_FLAG_ALT  0x02
/* The frame is on the top level */
#define FRAME_FLAG_LEAF  0x04
/* The frame can be internal alt frame */
#define FRAME_FLAG_ALT_INL  0x08
/* The frame update the DPB reference */
#define FRAME_FLAG_UPDATE_REF  0x10
/* The frame is the last frame in a GF group */
#define FRAME_FLAG_LAST_IN_GF  0x20
/* The frame has already be encoded */
#define FRAME_FLAG_ALREADY_ENCODED  0x40
/* The frame has already outputted */
#define FRAME_FLAG_ALREADY_OUTPUTTED  0x80
/* The frame not show */
#define FRAME_FLAG_NOT_SHOW  0x100
/* The frame is inside a TU cache */
#define FRAME_FLAG_FRAME_IN_TU_CACHE  0x200

#define MAX_ORDER_HINT_BITS_MINUS_1 7

struct _GstVaAV1GFGroup
{
  /* where this GF group start since key frame. */
  gint start_frame_offset;
  /* Total frame number of this group. */
  gint group_frame_num;
  /* Be different from group_frame_num, include repeat */
  gint output_frame_num;
  gint last_pushed_num;
  gint last_poped_index;
  guint8 highest_level;
  gboolean use_alt;
  gboolean intra_only;
  GQueue *reorder_list;

  /* Include FRAME_TYPEs or FRAME_TYPE_REPEAT. */
  GstAV1FrameType frame_types[MAX_GF_GROUP_SIZE * 2];
  gint8 pyramid_levels[MAX_GF_GROUP_SIZE * 2];
  guint32 flags[MAX_GF_GROUP_SIZE * 2];
  /* offset from start_frame_num. */
  gint frame_offsets[MAX_GF_GROUP_SIZE * 2];
};

struct _GstVaAV1Ref
{
  GstVideoCodecFrame *frame;
  guint index_in_dpb;
};

struct _GstVaAV1EncFrame
{
  GstVaEncodePicture *picture;
  GstAV1FrameType type;
  guint8 temporal_id;
  guint8 spatial_id;
  /* AV1 does not define a frame number.
     This is a virtual number after the key frame. */
  gint frame_num;
  guint32 flags;
  guint pyramid_level;
  gboolean bidir_ref;
  gint8 ref_frame_idx[GST_AV1_NUM_REF_FRAMES];
  /* The index in reference list to update */
  gint update_index;
  gint order_hint;
  /* The current frame to repeat */
  gint8 repeat_index;

  guint cached_frame_header_size;
  guint8 cached_frame_header[32];
};

struct _GstVaAV1Enc
{
  /*< private > */
  GstVaBaseEnc parent;

  guint32 packed_headers;

  guint mi_rows;
  guint mi_cols;
  gint level_idx;
  const gchar *level_str;
  guint tier;
  guint cr;
  guint depth;
  guint chrome;
  GstClockTime last_pts;
  GstClockTime last_dts;

  /* properties */
  struct
  {
    /* kbps */
    guint bitrate;
    /* VA_RC_XXX */
    guint rc_ctrl;
    guint32 target_usage;
    guint32 cpb_size;
    guint32 target_percentage;
    guint gf_group_size;
    guint num_ref_frames;
    guint max_hierarchical_level;
    gboolean use_128x128_superblock;
    guint keyframe_interval;
    guint32 qp;
    guint32 min_qp;
    guint32 max_qp;
    guint32 num_tile_cols;
    guint32 num_tile_rows;
    guint32 tile_groups;
    guint32 mbbrc;
  } prop;

  struct
  {
    guint keyframe_interval;
    guint gf_group_size;
    guint max_level;
    guint num_ref_frames;
    /* Forward only(P kind frame) may have diff refs num in l0 */
    guint forward_only_ref_num;
    guint forward_ref_num;
    guint backward_ref_num;
    guint frame_num_since_kf;
    gboolean enable_order_hint;
    GstVaAV1GFGroup current_group;
    GstVideoCodecFrame *last_keyframe;
    GstVideoCodecFrame *ref_list[GST_AV1_NUM_REF_FRAMES];
  } gop;

  struct
  {
    guint sb_rows;
    guint sb_cols;
    gboolean use_128x128_superblock;
    guint32 num_tile_cols;
    guint32 num_tile_rows;
    guint32 tile_groups;
    guint32 tile_cols_log2;
    guint32 tile_rows_log2;
    gboolean uniform;
    guint32 tile_width_sb;
    guint32 tile_height_sb;
    /* To calculate tile size bytes in tile group obu */
    guint32 tile_size_bytes_minus_1;
    guint32 max_tile_num;
  } partition;

  struct
  {
    guint target_usage;
    guint32 target_percentage;
    guint32 cpb_size;
    guint32 cpb_length_bits;
    guint32 rc_ctrl_mode;
    guint max_bitrate;
    guint max_bitrate_bits;
    guint target_bitrate;
    guint target_bitrate_bits;
    guint32 base_qindex;
    guint32 min_qindex;
    guint32 max_qindex;
    guint32 mbbrc;
  } rc;

  struct
  {
    gboolean enable_cdef;
    gboolean cdef_channel_strength;
    gboolean enable_filter_intra;
    gboolean enable_intra_edge_filter;
    gboolean enable_interintra_compound;
    gboolean enable_masked_compound;
    gboolean enable_warped_motion;
    gboolean enable_palette_mode;
    gboolean enable_dual_filter;
    gboolean enable_jnt_comp;
    gboolean enable_ref_frame_mvs;
    gboolean enable_superres;
    gboolean enable_restoration;
    gboolean allow_intrabc;
    gboolean enable_segmentation;
    /* (1 << interpolation_filter) means support not not. */
    guint32 interpolation_filter_support;
    /* The interpolation type we choose */
    GstAV1InterpolationFilter interpolation_type;
    /* The size field bytes in obu header */
    guint obu_size_bytes;
    /* (tx_mode_support & mode) == 1 means support the mode. */
    guint tx_mode_support;
  } features;

  /* The cached frames in the same TU. */
  GstVideoCodecFrame *frames_in_tu[GST_AV1_NUM_REF_FRAMES - 1];
  guint frames_in_tu_num;
  gboolean inside_tu;

  GstAV1SequenceHeaderOBU sequence_hdr;
};

struct _GstVaAV1EncClass
{
  GstVaBaseEncClass parent_class;

  GType rate_control_type;
  char rate_control_type_name[64];
  GEnumValue rate_control[16];
};

/**
 * GstVaAV1LevelLimits:
 * @level_name: the level name
 * @seq_level_idx: specifies the level index value
 * @MaxPicSize: the maximum of picture size in samples
 * @MaxHSize: the maximum of picture width in samples
 * @MaxVSize: the maximum of picture height in samples
 * @MaxDisplayRate: the maximum of display luma samples rate per second
 * @MaxDecodeRate: the maximum of decode luma samples rate per second
 * @MaxHeaderRate: the maximum number of frame/frame_header per second
 * @MainMbps: the maximum bit rate in for main tier
 * @HighMbps: the maximum bit rate in for high tier
 * @MainCR: the minimum picture compress ratio for main tier
 * @HighCR: the minimum picture compress ratio for high tier
 * @MaxTiles: the maximum tile number
 * @MaxTileCols: the maximum tile number in column
 *
 * The data structure that describes the limits of an AV1 level.
 */
struct _GstVaAV1LevelLimits
{
  const gchar *level_name;
  guint8 seq_level_idx;
  guint32 MaxPicSize;
  guint32 MaxHSize;
  guint32 MaxVSize;
  guint64 MaxDisplayRate;
  guint64 MaxDecodeRate;
  guint32 MaxHeaderRate;
  guint32 MainMbps;
  guint32 HighMbps;
  guint32 MainCR;
  guint32 HighCR;
  guint32 MaxTiles;
  guint32 MaxTileCols;
};

/* A.3. Levels */
/* *INDENT-OFF* */
static const GstVaAV1LevelLimits _va_av1_level_limits[] = {
/* level idx MaxPicSize MaxHSize MaxVSize MaxDisplayRate MaxDecodeRate MaxHeaderRate MainMbps  HighMbps  MainCR HighCR MaxTiles MaxTileCols*/
  {"2.0", 0, 147456,    2048,    1152,    4423680,       5529600,      150,          1500000,  0,        2,     0,     8,       4           },
  {"2.1", 1, 278784,    2816,    1584,    8363520,       10454400,     150,          3000000,  0,        2,     0,     8,       4           },
  {"3.0", 4, 665856,    4352,    2448,    19975680,      24969600,     150,          6000000,  0,        2,     0,     16,      6           },
  {"3.1", 5, 1065024,   5504,    3096,    31950720,      39938400,     150,          10000000, 0,        2,     0,     16,      6           },
  {"4.0", 8, 2359296,   6144,    3456,    70778880,      77856768,     300,          12000000, 30000000, 4,     4,     32,      8           },
  {"4.1", 9, 2359296,   6144,    3456,    141557760,     155713536,    300,          20000000, 50000000, 4,     4,     32,      8           },
  {"5.0", 12,8912896,   8192,    4352,    267386880,     273715200,    300,          30000000, 100000000,6,     4,     64,      8           },
  {"5.1", 13,8912896,   8192,    4352,    534773760,     547430400,    300,          40000000, 160000000,8,     4,     64,      8           },
  {"5.2", 14,8912896,   8192,    4352,    1069547520,    1094860800,   300,          60000000, 240000000,8,     4,     64,      8           },
  {"5.3", 15,8912896,   8192,    4352,    1069547520,    1176502272,   300,          60000000, 240000000,8,     4,     64,      8           },
  {"6.0", 16,35651584,  16384,   8704,    1069547520,    1176502272,   300,          60000000, 240000000,8,     4,     128,     16          },
  {"6.1", 17,35651584,  16384,   8704,    2139095040,    2189721600,   300,          100000000,480000000,8,     4,     128,     16          },
  {"6.2", 18,35651584,  16384,   8704,    4278190080,    4379443200,   300,          160000000,800000000,8,     4,     128,     16          },
  {"6.3", 19,35651584,  16384,   8704,    4278190080,    4706009088,   300,          160000000,800000000,8,     4,     128,     16          },
};
/* *INDENT-ON* */

static gboolean
_av1_calculate_level_and_tier (GstVaAV1Enc * self)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  gint pic_size;
  guint64 display_rate /* TotalDisplayLumaSampleRate */ ;
  guint max_bitrate;
  guint tier = 0;
  int i;

  pic_size = base->width * base->height;
  display_rate = gst_util_uint64_scale_int_ceil (pic_size,
      GST_VIDEO_INFO_FPS_N (&base->in_info),
      GST_VIDEO_INFO_FPS_D (&base->in_info));

  for (i = 0; i < G_N_ELEMENTS (_va_av1_level_limits); i++) {
    const GstVaAV1LevelLimits *limits = &_va_av1_level_limits[i];

    tier = 0;

    if (pic_size > limits->MaxPicSize)
      continue;

    if (base->width > limits->MaxHSize)
      continue;

    if (base->height > limits->MaxVSize)
      continue;

    if (display_rate > limits->MaxDisplayRate)
      continue;

    max_bitrate = limits->HighMbps != 0 ? limits->HighMbps : limits->MainMbps;
    if (self->rc.max_bitrate > max_bitrate)
      continue;
    if (self->rc.max_bitrate > limits->MainMbps)
      tier = 1;

    if (self->partition.num_tile_cols * self->partition.num_tile_rows >
        limits->MaxTiles)
      continue;

    if (self->partition.num_tile_cols > limits->MaxTileCols)
      continue;

    /* decode rate, header rate, compress rate, etc. are not considered. */
    break;
  }

  if (i == G_N_ELEMENTS (_va_av1_level_limits)) {
    GST_ERROR_OBJECT (self,
        "failed to find suitable level and tier matching codec config");
    return FALSE;
  }

  self->level_idx = _va_av1_level_limits[i].seq_level_idx;
  self->level_str = _va_av1_level_limits[i].level_name;
  self->tier = tier;
  self->cr =
      tier ? _va_av1_level_limits[i].HighCR : _va_av1_level_limits[i].MainCR;
  g_assert (self->cr > 0);

  GST_INFO_OBJECT (self, "Use level: %s, tier is %d, cr is %d",
      self->level_str, self->tier, self->cr);
  return TRUE;
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

static inline GstVaAV1EncFrame *
_enc_frame (GstVideoCodecFrame * frame)
{
  GstVaAV1EncFrame *enc_frame = gst_video_codec_frame_get_user_data (frame);

  g_assert (enc_frame);

  return enc_frame;
}

#ifndef GST_DISABLE_GST_DEBUG
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

static GstVaAV1EncFrame *
gst_va_av1_enc_frame_new (void)
{
  GstVaAV1EncFrame *frame;

  frame = g_slice_new (GstVaAV1EncFrame);
  frame->frame_num = -1;
  frame->type = FRAME_TYPE_INVALID;
  frame->temporal_id = 0;
  frame->spatial_id = 0;
  frame->picture = NULL;
  frame->pyramid_level = 0;
  frame->flags = 0;
  frame->bidir_ref = FALSE;
  frame->update_index = -1;
  frame->order_hint = -1;
  frame->repeat_index = -1;
  frame->cached_frame_header_size = 0;

  return frame;
}

static void
gst_va_av1_enc_frame_free (gpointer pframe)
{
  GstVaAV1EncFrame *frame = pframe;

  g_clear_pointer (&frame->picture, gst_va_encode_picture_free);
  g_slice_free (GstVaAV1EncFrame, frame);
}

static gboolean
gst_va_av1_enc_new_frame (GstVaBaseEnc * base, GstVideoCodecFrame * frame)
{
  GstVaAV1EncFrame *frame_in;

  frame_in = gst_va_av1_enc_frame_new ();
  gst_video_codec_frame_set_user_data (frame, frame_in,
      gst_va_av1_enc_frame_free);

  return TRUE;
}

#ifndef GST_DISABLE_GST_DEBUG
static const char *
_av1_get_frame_type_name (GstAV1FrameType frame_type)
{
  const gchar *frame_type_name = NULL;
  guint type = frame_type;

  if (type & FRAME_TYPE_REPEAT)
    return "Repeat ";

  switch (type) {
    case GST_AV1_KEY_FRAME:
      frame_type_name = "Key    ";
      break;
    case GST_AV1_INTER_FRAME:
      frame_type_name = "Inter  ";
      break;
    case GST_AV1_INTRA_ONLY_FRAME:
      frame_type_name = "Intra  ";
      break;
    case GST_AV1_SWITCH_FRAME:
      frame_type_name = "Switch ";
      break;
    default:
      frame_type_name = "Unknown";
      break;
  }

  return frame_type_name;
}
#endif

static void
_av1_print_gf_group (GstVaAV1Enc * self, GstVaAV1GFGroup * gf_group)
{
#ifndef GST_DISABLE_GST_DEBUG
  gint pushed_frame_num = gf_group->last_pushed_num < 0 ? 0 :
      gf_group->last_pushed_num - gf_group->start_frame_offset + 1;
  GString *str;
  gint i;

  if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) < GST_LEVEL_LOG)
    return;

  str = g_string_new (NULL);

  g_string_append_printf (str, "\n============================"
      " GF Group ===========================\n");
  g_string_append_printf (str, " start:%d,  size:%d  ",
      gf_group->start_frame_offset, gf_group->group_frame_num);
  g_string_append_printf (str, "pushed:%d,  poped:%d  ",
      pushed_frame_num, gf_group->last_poped_index + 1);
  g_string_append_printf (str, "\n ALT: %s  max level: %d  output num: %d",
      gf_group->use_alt ? "yes" : "no", gf_group->highest_level,
      gf_group->output_frame_num);

  g_string_append_printf (str, "\n----------------------------"
      "-------------------------------------\n");
  g_string_append_printf (str, "|     |  type   | level |"
      "             flags            | offset |\n");

  g_string_append_printf (str, "----------------------------"
      "-------------------------------------\n");
  for (i = 0; i < gf_group->output_frame_num; i++) {
    g_string_append_printf (str, "| %3d ", i);
    g_string_append_printf (str, "| %s ",
        _av1_get_frame_type_name (gf_group->frame_types[i]));

    g_string_append_printf (str, "| %5d | ", gf_group->pyramid_levels[i]);

    if (gf_group->flags[i] & FRAME_FLAG_GF) {
      g_string_append_printf (str, "GF ");
    } else {
      g_string_append_printf (str, "   ");
    }

    if (gf_group->flags[i] & FRAME_FLAG_LAST_IN_GF) {
      g_string_append_printf (str, "Last ");
    } else {
      g_string_append_printf (str, "     ");
    }

    if (gf_group->flags[i] & (FRAME_FLAG_ALT | FRAME_FLAG_ALT_INL)) {
      g_string_append_printf (str, "ALT ");
    } else {
      g_string_append_printf (str, "    ");
    }

    if (gf_group->flags[i] & FRAME_FLAG_NOT_SHOW) {
      g_string_append_printf (str, "Unshown ");
    } else {
      g_string_append_printf (str, "Shown   ");
    }

    if (gf_group->flags[i] & FRAME_FLAG_LEAF) {
      g_string_append_printf (str, "Leaf ");
    } else {
      g_string_append_printf (str, "     ");
    }

    if (gf_group->flags[i] & FRAME_FLAG_UPDATE_REF) {
      g_string_append_printf (str, "Ref ");
    } else {
      g_string_append_printf (str, "    ");
    }

    g_string_append_printf (str, "| %-5d  | ", gf_group->frame_offsets[i]);

    g_string_append_printf (str, "\n");
  }

  g_string_append_printf (str, "---------------------------"
      "--------------------------------------\n");

  GST_LOG_OBJECT (self, "%s", str->str);

  g_string_free (str, TRUE);

#endif
}

#ifndef GST_DISABLE_GST_DEBUG
static void
_av1_print_ref_list (GstVaAV1Enc * self, GString * str)
{
  gint i;

  g_string_append_printf (str, "\n================== Reference List "
      "===================\n");

  g_string_append_printf (str, "|   index   |");
  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++)
    g_string_append_printf (str, "%3d |", i);

  g_string_append_printf (str, "\n-------------------------------"
      "----------------------\n");

  g_string_append_printf (str, "| frame num |");
  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
    if (self->gop.ref_list[i]) {
      GstVaAV1EncFrame *va_frame = _enc_frame (self->gop.ref_list[i]);
      g_string_append_printf (str, "%3d |", va_frame->frame_num);
    } else {
      g_string_append_printf (str, "%3d |", -1);
    }
  }
  g_string_append_printf (str, "\n-------------------------------"
      "----------------------\n");
}
#endif

static void
_av1_print_frame_reference (GstVaAV1Enc * self, GstVideoCodecFrame * frame)
{
#ifndef GST_DISABLE_GST_DEBUG
  GString *str;
  GstVaAV1EncFrame *va_frame;
  gint i;

  if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) < GST_LEVEL_LOG)
    return;

  str = g_string_new (NULL);

  _av1_print_ref_list (self, str);

  va_frame = _enc_frame (frame);

  g_string_append_printf (str, "Current %sframe num: %d,  ",
      va_frame->frame_num == 0 ? "key " : "", va_frame->frame_num);

  if (va_frame->type & FRAME_TYPE_REPEAT) {
    g_string_append_printf (str, "repeat index %d", va_frame->repeat_index);
    goto print;
  }

  g_string_append_printf (str, "Reference: [");

  for (i = GST_AV1_REF_LAST_FRAME; i < GST_AV1_NUM_REF_FRAMES; i++) {
    switch (i) {
      case GST_AV1_REF_LAST_FRAME:
        g_string_append_printf (str, " %s", "Last");
        break;
      case GST_AV1_REF_LAST2_FRAME:
        g_string_append_printf (str, " %s", "Last2");
        break;
      case GST_AV1_REF_LAST3_FRAME:
        g_string_append_printf (str, " %s", "Last3");
        break;
      case GST_AV1_REF_GOLDEN_FRAME:
        g_string_append_printf (str, " %s", "Golden");
        break;
      case GST_AV1_REF_BWDREF_FRAME:
        g_string_append_printf (str, " %s", "Bwd");
        break;
      case GST_AV1_REF_ALTREF2_FRAME:
        g_string_append_printf (str, " %s", "Alt2");
        break;
      case GST_AV1_REF_ALTREF_FRAME:
        g_string_append_printf (str, " %s", "Alt");
        break;
      default:
        g_assert_not_reached ();
        break;
    }
    g_string_append_printf (str, ":");

    if (va_frame->ref_frame_idx[i] == -1) {
      g_string_append_printf (str, "unused");
    } else {
      g_string_append_printf (str, "%d", va_frame->ref_frame_idx[i]);
    }

    if (i != GST_AV1_NUM_REF_FRAMES - 1) {
      g_string_append_printf (str, ", ");
    } else {
      g_string_append_printf (str, " ");
    }
  }

  g_string_append_printf (str, "]");

print:
  GST_LOG_OBJECT (self, "%s", str->str);

  g_string_free (str, TRUE);
#endif
}

static void
_av1_print_ref_list_update (GstVaAV1Enc * self, gint update_index,
    GstVideoCodecFrame * del_frame, GstVideoCodecFrame * add_frame)
{
#ifndef GST_DISABLE_GST_DEBUG
  GString *str;

  if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) < GST_LEVEL_LOG)
    return;

  str = g_string_new (NULL);

  _av1_print_ref_list (self, str);

  if (_enc_frame (add_frame)->frame_num == 0)
    g_string_append_printf (str, "Key frame clear all reference list.\n");

  if (update_index >= 0) {
    if (del_frame) {
      g_string_append_printf (str, "Replace index %d, delete frame num %d, "
          "add frame num %d.", update_index, _enc_frame (del_frame)->frame_num,
          _enc_frame (add_frame)->frame_num);
    } else {
      g_string_append_printf (str, "Add frame %d to index %d.",
          _enc_frame (add_frame)->frame_num, update_index);
    }
  }

  GST_LOG_OBJECT (self, "%s", str->str);

  g_string_free (str, TRUE);
#endif
}

/* [ start, end ) */
static void
_set_multi_layer (GstVaAV1GFGroup * gf_group, gint * frame_index,
    gint start, gint end, guint level, guint max_level)
{
  const gint num_frames_to_process = end - start;
  guint m = (start + end - 1) / 2;

  g_assert (level <= max_level);

  if (level == max_level || num_frames_to_process <= 2) {
    guint i;

    for (i = 0; i < num_frames_to_process; i++) {
      gf_group->frame_types[*frame_index] = GST_AV1_INTER_FRAME;
      gf_group->pyramid_levels[*frame_index] = level;
      gf_group->flags[*frame_index] = FRAME_FLAG_LEAF | FRAME_FLAG_UPDATE_REF;
      gf_group->frame_offsets[*frame_index] = start + i;
      (*frame_index)++;
    }
    return;
  }

  gf_group->frame_types[*frame_index] = GST_AV1_INTER_FRAME;
  gf_group->pyramid_levels[*frame_index] = level;
  gf_group->flags[*frame_index] = FRAME_FLAG_ALT_INL | FRAME_FLAG_UPDATE_REF;
  gf_group->flags[*frame_index] |= FRAME_FLAG_NOT_SHOW;
  gf_group->frame_offsets[*frame_index] = m;
  (*frame_index)++;

  /* The frames between start and this internal ALT */
  _set_multi_layer (gf_group, frame_index, start, m, level + 1, max_level);

  /* Repeat for this internal ALT frame */
  gf_group->frame_types[*frame_index] = FRAME_TYPE_REPEAT;
  gf_group->pyramid_levels[*frame_index] = -1;
  gf_group->flags[*frame_index] = 0;
  gf_group->frame_offsets[*frame_index] = m;
  (*frame_index)++;

  /* The frames between this internal ALT and end */
  _set_multi_layer (gf_group, frame_index, m + 1, end, level + 1, max_level);
}

static void
_av1_init_gf_group (GstVaAV1GFGroup * gf_group, GQueue * reorder_list)
{
  guint i;

  gf_group->start_frame_offset = -1;
  gf_group->group_frame_num = 0;
  gf_group->last_pushed_num = -1;
  gf_group->use_alt = FALSE;
  gf_group->intra_only = FALSE;
  gf_group->last_poped_index = -1;
  gf_group->output_frame_num = 0;

  for (i = 0; i < MAX_GF_GROUP_SIZE * 2; i++) {
    gf_group->frame_types[i] = FRAME_TYPE_INVALID;
    gf_group->pyramid_levels[i] = INVALID_PYRAMID_LEVEL;
    gf_group->flags[i] = 0;
    gf_group->frame_offsets[i] = -1;
  }

  /* We just use it, not own it. */
  gf_group->reorder_list = reorder_list;
}

static void
_av1_start_gf_group (GstVaAV1Enc * self, GstVideoCodecFrame * gf_frame)
{
  GstVaAV1GFGroup *gf_group = &self->gop.current_group;
  guint group_size = self->gop.gf_group_size + 1;
  gboolean use_alt = self->gop.backward_ref_num > 0;
  gboolean intra_only = (self->gop.num_ref_frames == 0);
  guint max_level = self->gop.max_level;
  GstVaAV1EncFrame *frame = _enc_frame (gf_frame);
  gboolean key_frame_start = (frame->frame_num == 0);
  gint frame_index;
  guint i;

  if (use_alt) {
    /* At least 2 levels if bi-direction ref,
       1st for ALT, and 2nd for leaves. */
    g_assert (max_level >= 2);
    g_assert (intra_only == FALSE);
  }

  /* + 1 for golden frame itself. */
  g_assert (group_size <= MAX_GF_GROUP_SIZE + 1);
  g_assert (max_level <= HIGHEST_PYRAMID_LEVELS);
  /* If size < 3, no backward ref needed. */
  g_assert (group_size > 3 || use_alt == FALSE);

  gf_group->start_frame_offset = frame->frame_num;
  gf_group->group_frame_num = group_size;
  gf_group->last_pushed_num = frame->frame_num;
  gf_group->use_alt = use_alt;
  gf_group->intra_only = intra_only;

  gf_group->last_poped_index = -1;
  /* An already encoded frame as the GF,
     for example, the ALT of the previous GF group. */
  if (frame->flags & FRAME_FLAG_ALREADY_ENCODED)
    gf_group->last_poped_index = 0;

  for (i = 0; i < MAX_GF_GROUP_SIZE * 2; i++) {
    gf_group->frame_types[i] = FRAME_TYPE_INVALID;
    gf_group->pyramid_levels[i] = INVALID_PYRAMID_LEVEL;
    gf_group->flags[i] = 0;
    gf_group->frame_offsets[i] = -1;
  }

  frame_index = 0;
  /* GF frame is the first */
  gf_group->frame_types[frame_index] = key_frame_start ? GST_AV1_KEY_FRAME :
      intra_only ? GST_AV1_INTRA_ONLY_FRAME : GST_AV1_INTER_FRAME;
  gf_group->pyramid_levels[frame_index] = 0;
  gf_group->flags[frame_index] = FRAME_FLAG_GF;
  gf_group->flags[frame_index] |= FRAME_FLAG_UPDATE_REF;
  gf_group->frame_offsets[frame_index] = 0;
  frame_index++;

  /* No backward ref, in simple I/P mode */
  if (gf_group->use_alt == FALSE) {
    for (; frame_index < gf_group->group_frame_num; frame_index++) {
      gf_group->frame_types[frame_index] =
          intra_only ? GST_AV1_INTRA_ONLY_FRAME : GST_AV1_INTER_FRAME;
      gf_group->pyramid_levels[frame_index] = 1;
      gf_group->flags[frame_index] = FRAME_FLAG_UPDATE_REF | FRAME_FLAG_LEAF;
      if (frame_index == gf_group->group_frame_num - 1)
        gf_group->flags[frame_index] |= FRAME_FLAG_LAST_IN_GF;
      gf_group->frame_offsets[frame_index] = frame_index;
    }

    gf_group->output_frame_num = gf_group->group_frame_num;
    gf_group->highest_level = 1;

    _av1_print_gf_group (self, gf_group);
    return;
  }

  /* ALT frame */
  gf_group->frame_types[frame_index] = GST_AV1_INTER_FRAME;
  gf_group->pyramid_levels[frame_index] = 1;
  gf_group->flags[frame_index] = FRAME_FLAG_ALT | FRAME_FLAG_UPDATE_REF;
  gf_group->flags[frame_index] |= FRAME_FLAG_LAST_IN_GF;
  gf_group->flags[frame_index] |= FRAME_FLAG_NOT_SHOW;
  gf_group->frame_offsets[frame_index] = gf_group->group_frame_num - 1;
  frame_index++;

  /* The frames between GF and ALT */
  _set_multi_layer (gf_group, &frame_index, 1,
      gf_group->group_frame_num - 1, 2, max_level);

  /* Repeat for ALT frame */
  gf_group->frame_types[frame_index] = FRAME_TYPE_REPEAT;
  gf_group->pyramid_levels[frame_index] = -1;
  gf_group->flags[frame_index] = 0;
  gf_group->frame_offsets[frame_index] = gf_group->group_frame_num - 1;
  frame_index++;

  gf_group->output_frame_num = frame_index;

  gf_group->highest_level = 0;
  for (i = 0; i < gf_group->output_frame_num; i++) {
    if (gf_group->highest_level < gf_group->pyramid_levels[i])
      gf_group->highest_level = gf_group->pyramid_levels[i];
  }

  _av1_print_gf_group (self, gf_group);
  return;
}

static gboolean
_av1_gf_group_push_frame (GstVaAV1GFGroup * gf_group,
    GstVideoCodecFrame * gst_frame)
{
  GstVaAV1EncFrame *frame = _enc_frame (gst_frame);
  gint pushed_frame_num = gf_group->last_pushed_num < 0 ? 0 :
      gf_group->last_pushed_num - gf_group->start_frame_offset + 1;

  /* No room for a new one. */
  g_return_val_if_fail (pushed_frame_num < gf_group->group_frame_num, FALSE);
  /* The frame num should just increase. */
  g_return_val_if_fail (frame->frame_num == gf_group->last_pushed_num + 1,
      FALSE);

  if (gf_group->use_alt)
    /* If we already begin pop, no more push again. */
    g_return_val_if_fail (gf_group->last_poped_index <= 0, FALSE);

  g_queue_push_tail (gf_group->reorder_list,
      gst_video_codec_frame_ref (gst_frame));

  gf_group->last_pushed_num = frame->frame_num;
  return TRUE;
}

static gboolean
_av1_gf_group_pop_frame (GstVaAV1GFGroup * gf_group,
    GstVideoCodecFrame * ref_list[GST_AV1_NUM_REF_FRAMES],
    GstVideoCodecFrame ** out_frame)
{
  GstVaAV1EncFrame *vaframe;
  GstVideoCodecFrame *frame;
  gint pushed_frame_num = gf_group->last_pushed_num < 0 ? 0 :
      gf_group->last_pushed_num - gf_group->start_frame_offset + 1;
  guint i;

  g_assert (pushed_frame_num <= gf_group->group_frame_num);

  if (pushed_frame_num == 0)
    goto no_frame;

  if (!gf_group->use_alt) {
    g_assert (gf_group->last_poped_index < pushed_frame_num);

    if (gf_group->last_poped_index + 1 < pushed_frame_num) {
      gf_group->last_poped_index++;
      goto find_frame;
    }

    goto no_frame;
  }

  /* The first frame of a GF group has no backward ref, pop immediately. */
  if (gf_group->last_poped_index < 0) {
    gf_group->last_poped_index++;
    goto find_frame;
  }

  /* The ALT frame has not come. */
  if (pushed_frame_num < gf_group->group_frame_num)
    goto no_frame;

  gf_group->last_poped_index++;
  g_assert (gf_group->last_poped_index < gf_group->output_frame_num);

find_frame:
  vaframe = NULL;
  frame = NULL;

  /* If repeating some frame, it should be in reference list,
     or it should be in reorder list. */
  if (gf_group->frame_types[gf_group->last_poped_index] == FRAME_TYPE_REPEAT) {
    for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
      GstVaAV1EncFrame *vaf;

      if (ref_list[i] == NULL)
        continue;

      vaf = _enc_frame (ref_list[i]);
      if (vaf->frame_num == gf_group->start_frame_offset +
          gf_group->frame_offsets[gf_group->last_poped_index]) {
        vaframe = vaf;
        frame = ref_list[i];
        break;
      }
    }

    g_return_val_if_fail (vaframe, FALSE);

    g_assert (vaframe->flags & FRAME_FLAG_ALREADY_ENCODED);
    vaframe->type |= FRAME_TYPE_REPEAT;
  } else {
    for (i = 0; i < g_queue_get_length (gf_group->reorder_list); i++) {
      GstVaAV1EncFrame *vaf;
      GstVideoCodecFrame *f;

      f = g_queue_peek_nth (gf_group->reorder_list, i);
      vaf = _enc_frame (f);
      if (vaf->frame_num == gf_group->start_frame_offset +
          gf_group->frame_offsets[gf_group->last_poped_index]) {
        vaframe = vaf;
        frame = f;
        break;
      }
    }
    /* We push the frame num in increment order, so it must exist. */
    g_return_val_if_fail (vaframe, FALSE);
    /* Clear that frame from reorder list. */
    g_queue_pop_nth (gf_group->reorder_list, i);

    vaframe->type = gf_group->frame_types[gf_group->last_poped_index];
    vaframe->pyramid_level =
        gf_group->pyramid_levels[gf_group->last_poped_index];
    vaframe->flags = gf_group->flags[gf_group->last_poped_index];

    /* unref frame popped from reorder queue */
    gst_video_codec_frame_unref (frame);
  }

  *out_frame = frame;
  return TRUE;

no_frame:
  *out_frame = NULL;
  return TRUE;
}

/* Force to finish current group, no matter how many frames we have. */
static void
_av1_finish_current_gf_group (GstVaAV1Enc * self, GstVaAV1GFGroup * gf_group)
{
  gint frame_index;
  gint pushed_frame_num = gf_group->last_pushed_num < 0 ? 0 :
      gf_group->last_pushed_num - gf_group->start_frame_offset + 1;
  guint i;

  g_assert (pushed_frame_num <= gf_group->group_frame_num);

  /* Alt comes and already finished. */
  if (gf_group->use_alt && gf_group->last_poped_index > 0)
    return;

  /* Already pushed all frames. */
  if (pushed_frame_num == gf_group->group_frame_num)
    return;

  /* Not enough frames, no need to use backward ref. */
  if (gf_group->use_alt && pushed_frame_num <= 3)
    gf_group->use_alt = FALSE;

  if (gf_group->use_alt == FALSE) {
    g_assert (gf_group->last_poped_index < pushed_frame_num);

    gf_group->group_frame_num = pushed_frame_num;

    for (frame_index = 1; frame_index < gf_group->group_frame_num;
        frame_index++) {
      gf_group->frame_types[frame_index] =
          gf_group->intra_only ? GST_AV1_INTRA_ONLY_FRAME : GST_AV1_INTER_FRAME;
      gf_group->pyramid_levels[frame_index] = 1;
      gf_group->flags[frame_index] = FRAME_FLAG_UPDATE_REF | FRAME_FLAG_LEAF;
      gf_group->frame_offsets[frame_index] = frame_index;
      if (frame_index == gf_group->group_frame_num - 1) {
        gf_group->flags[frame_index] |= FRAME_FLAG_LAST_IN_GF;
      }

    }

    gf_group->output_frame_num = gf_group->group_frame_num;
    gf_group->highest_level = 1;

    GST_LOG_OBJECT (self, "Finish current golden group.");
    _av1_print_gf_group (self, gf_group);
    return;
  }

  g_assert (gf_group->highest_level >= 2);

  gf_group->group_frame_num = pushed_frame_num;

  frame_index = 1;
  /* ALT frame */
  gf_group->frame_types[frame_index] = GST_AV1_INTER_FRAME;
  gf_group->pyramid_levels[frame_index] = 1;
  gf_group->flags[frame_index] = FRAME_FLAG_ALT | FRAME_FLAG_UPDATE_REF;
  gf_group->flags[frame_index] |= FRAME_FLAG_LAST_IN_GF;
  gf_group->flags[frame_index] |= FRAME_FLAG_NOT_SHOW;
  gf_group->frame_offsets[frame_index] = gf_group->group_frame_num - 1;
  frame_index++;

  /* The frames between GF and ALT */
  _set_multi_layer (gf_group, &frame_index, 1, gf_group->group_frame_num - 1,
      2, gf_group->highest_level);

  /* Repeat of ALT frame */
  gf_group->frame_types[frame_index] = FRAME_TYPE_REPEAT;
  gf_group->pyramid_levels[frame_index] = -1;
  gf_group->flags[frame_index] = 0;
  gf_group->frame_offsets[frame_index] = gf_group->group_frame_num - 1;
  frame_index++;

  gf_group->output_frame_num = frame_index;

  gf_group->highest_level = 0;
  for (i = 0; i < gf_group->output_frame_num; i++) {
    if (gf_group->highest_level < gf_group->pyramid_levels[i])
      gf_group->highest_level = gf_group->pyramid_levels[i];
  }

  GST_LOG_OBJECT (self, "Finish current golden group.");
  _av1_print_gf_group (self, gf_group);
  return;
}

static inline gboolean
_av1_gf_group_is_empty (GstVaAV1GFGroup * gf_group)
{
  gint pushed_frame_num = gf_group->last_pushed_num < 0 ? 0 :
      gf_group->last_pushed_num - gf_group->start_frame_offset + 1;

  if (pushed_frame_num <= 0)
    return TRUE;

  if (gf_group->use_alt == FALSE) {
    g_assert (gf_group->last_poped_index + 1 <= pushed_frame_num);
    if (gf_group->last_poped_index + 1 == pushed_frame_num)
      return TRUE;

    return FALSE;
  }

  g_assert (gf_group->last_poped_index < gf_group->output_frame_num);
  if (gf_group->last_poped_index == gf_group->output_frame_num - 1)
    return TRUE;

  return FALSE;
}

static inline gboolean
_av1_gf_group_is_finished (GstVaAV1GFGroup * gf_group)
{
  g_assert (gf_group->last_poped_index < gf_group->output_frame_num);
  if (gf_group->last_poped_index == gf_group->output_frame_num - 1)
    return TRUE;

  return FALSE;
}

static GstVideoCodecFrame *
_av1_find_next_golden_frame (GstVaAV1Enc * self)
{
  guint i;
  GstVideoCodecFrame *f, *f_max_frame_num;
  GstVaAV1EncFrame *vaf;
  gint max_frame_num;

  g_assert (_av1_gf_group_is_empty (&self->gop.current_group));

  f = NULL;
  f_max_frame_num = NULL;
  max_frame_num = -1;
  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
    if (self->gop.ref_list[i] == NULL)
      continue;

    vaf = _enc_frame (self->gop.ref_list[i]);
    if (vaf->flags & FRAME_FLAG_LAST_IN_GF) {
      /* Should not have 2 of group end frame at the same time. */
      g_assert (f == NULL);
      f = self->gop.ref_list[i];
    }

    if (vaf->frame_num > max_frame_num) {
      max_frame_num = vaf->frame_num;
      f_max_frame_num = self->gop.ref_list[i];
    }

    /* clear all flags about last GF group. */
    vaf->flags &= ~(FRAME_FLAG_LAST_IN_GF | FRAME_FLAG_ALT_INL |
        FRAME_FLAG_ALT | FRAME_FLAG_GF);
  }

  if (f == NULL)
    f = f_max_frame_num;

  vaf = _enc_frame (f);
  vaf->flags |= FRAME_FLAG_GF;

  GST_LOG_OBJECT (self, "Find the next golden frame num %d", vaf->frame_num);

  return f;
}

static gboolean
gst_va_av1_enc_reorder_frame (GstVaBaseEnc * base, GstVideoCodecFrame * frame,
    gboolean bump_all, GstVideoCodecFrame ** out_frame)
{
  GstVaAV1Enc *self = GST_VA_AV1_ENC (base);
  GstVaAV1EncFrame *va_frame;

  *out_frame = NULL;

  if (bump_all) {
    g_return_val_if_fail (frame == NULL, FALSE);

    _av1_finish_current_gf_group (self, &self->gop.current_group);

    if (!_av1_gf_group_is_finished (&self->gop.current_group)) {
      g_assert (!_av1_gf_group_is_empty (&self->gop.current_group));
      goto pop;
    }

    /* no more frames, the cached key frame is the last frame */
    if (self->gop.last_keyframe) {
      g_assert (_av1_gf_group_is_empty (&self->gop.current_group));

      *out_frame = self->gop.last_keyframe;
      self->gop.last_keyframe = NULL;
    }

    goto finish;
  }

  /* Pop only. We can pop some frame if:
     1. The current GF group is not finished.
     2. Encountered a key frame last time and force to finish
     the current GF group. */
  if (frame == NULL) {
    if (!_av1_gf_group_is_empty (&self->gop.current_group))
      goto pop;

    if (self->gop.last_keyframe) {
      GstVideoCodecFrame *f = self->gop.last_keyframe;
      self->gop.last_keyframe = NULL;

      _av1_start_gf_group (self, f);
      goto pop;
    }

    goto finish;
  }

  if (self->gop.frame_num_since_kf == self->gop.keyframe_interval)
    self->gop.frame_num_since_kf = 0;

  if (GST_VIDEO_CODEC_FRAME_IS_FORCE_KEYFRAME (frame))
    self->gop.frame_num_since_kf = 0;

  va_frame = _enc_frame (frame);
  va_frame->frame_num = self->gop.frame_num_since_kf;
  self->gop.frame_num_since_kf++;

  GST_LOG_OBJECT (self, "push frame: system_frame_number %d, frame_num: %d",
      frame->system_frame_number, va_frame->frame_num);

  /* A new key frame force to finish the current GF group. */
  if (va_frame->frame_num == 0) {
    _av1_finish_current_gf_group (self, &self->gop.current_group);

    g_queue_push_tail (&base->reorder_list, gst_video_codec_frame_ref (frame));

    if (_av1_gf_group_is_finished (&self->gop.current_group)) {
      g_assert (_av1_gf_group_is_empty (&self->gop.current_group));

      /* Already poped all of the last group,
         so begin a new group with this keyframe. */
      _av1_start_gf_group (self, frame);
    } else {
      g_assert (!_av1_gf_group_is_empty (&self->gop.current_group));

      /* The reorder() should exhaust all available frames in the
         reorder list before push a frame again, so the last key
         frame should already be popped. */
      g_return_val_if_fail (self->gop.last_keyframe == NULL, FALSE);
      self->gop.last_keyframe = frame;
    }

    goto pop;
  }

  if (_av1_gf_group_is_finished (&self->gop.current_group)) {
    GstVideoCodecFrame *gf_frame;

    g_assert (_av1_gf_group_is_empty (&self->gop.current_group));

    gf_frame = _av1_find_next_golden_frame (self);
    /* At least, there are some frames inside the reference list. */
    g_assert (gf_frame);

    _av1_start_gf_group (self, gf_frame);
  }

  if (!_av1_gf_group_push_frame (&self->gop.current_group, frame)) {
    GST_WARNING_OBJECT (base, "Failed to push the frame,"
        " system_frame_number %d.", frame->system_frame_number);
    goto error;
  }

pop:
  frame = NULL;

  if (!_av1_gf_group_pop_frame (&self->gop.current_group, self->gop.ref_list,
          out_frame))
    goto error;

finish:
  if (*out_frame) {
    va_frame = _enc_frame (*out_frame);
    GST_LOG_OBJECT (self, "pop frame: system_frame_number %d,"
        " frame_num: %d, frame_type %s", (*out_frame)->system_frame_number,
        va_frame->frame_num, _av1_get_frame_type_name (va_frame->type));
  }

  return TRUE;

error:
  if (frame) {
    GST_ERROR_OBJECT (base, "Failed to reorder the frame,"
        " system_frame_number %d.", frame->system_frame_number);
  } else {
    GST_ERROR_OBJECT (base, "error when poping frame.");
  }
  return FALSE;
}

static gint
_av1_sort_by_frame_num (gconstpointer a, gconstpointer b, gpointer user_data)
{
  GstVaAV1EncFrame *frame1 = _enc_frame (((GstVaAV1Ref *) a)->frame);
  GstVaAV1EncFrame *frame2 = _enc_frame (((GstVaAV1Ref *) b)->frame);

  g_assert (frame1->frame_num != frame2->frame_num);

  return frame1->frame_num - frame2->frame_num;
}

static gboolean
_av1_assign_ref_index (GstVaAV1Enc * self, GstVideoCodecFrame * frame)
{
  GstVaAV1Ref all_refs[GST_AV1_NUM_REF_FRAMES];
  guint ref_num;
  gint forward_num, backward_num;
  gint forward_ref_num, backward_ref_num;;
  GstVaAV1EncFrame *va_frame = _enc_frame (frame);
  gint i, num;
  gboolean gf_assigned;

  memset (va_frame->ref_frame_idx, -1, sizeof (va_frame->ref_frame_idx));

  if (va_frame->type & FRAME_TYPE_REPEAT) {
    va_frame->repeat_index = -1;

    for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
      if (self->gop.ref_list[i] == frame) {
        va_frame->repeat_index = i;
        break;
      }
    }

    g_return_val_if_fail (va_frame->repeat_index >= 0, FALSE);
    goto finish;
  }

  /* key frame has no ref */
  if (va_frame->frame_num == 0) {
    g_assert (va_frame->type == GST_AV1_KEY_FRAME);
    va_frame->bidir_ref = FALSE;
    goto finish;
  }

  /* intra frame has no ref */
  if (va_frame->type == GST_AV1_INTRA_ONLY_FRAME) {
    va_frame->bidir_ref = FALSE;
    goto finish;
  }

  ref_num = forward_num = backward_num = 0;
  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
    GstVaAV1EncFrame *va_f;

    if (self->gop.ref_list[i] == NULL)
      continue;

    all_refs[ref_num].frame = self->gop.ref_list[i];
    all_refs[ref_num].index_in_dpb = i;
    ref_num++;

    va_f = _enc_frame (self->gop.ref_list[i]);
    g_assert (va_f->frame_num != va_frame->frame_num);
    if (va_f->frame_num < va_frame->frame_num) {
      forward_num++;
    } else {
      backward_num++;
      g_assert (va_f->flags & FRAME_FLAG_ALT ||
          va_f->flags & FRAME_FLAG_ALT_INL);
    }

    if (va_f->flags & FRAME_FLAG_GF)
      va_frame->ref_frame_idx[GST_AV1_REF_GOLDEN_FRAME] = i;
  }

  if (va_frame->ref_frame_idx[GST_AV1_REF_GOLDEN_FRAME] == -1) {
    GST_WARNING_OBJECT (self, "failed to find the golden frame.");
    return FALSE;
  }

  g_qsort_with_data (all_refs, ref_num, sizeof (GstVaAV1Ref),
      _av1_sort_by_frame_num, NULL);

  /* Assign the forward references in order of:
   * 1. The last frame which has the smallest diff.
   * 2. The golden frame which can be a key frame with better quality.
   * 3. The other frames by inverse frame number order.
   */
  va_frame->bidir_ref = FALSE;
  gf_assigned = FALSE;

  num = forward_num - 1;
  g_assert (num >= 0);
  /* if forward only, should use forward_only_ref_num */
  forward_ref_num = backward_num > 0 ?
      self->gop.forward_ref_num : self->gop.forward_only_ref_num;
  g_assert (forward_ref_num > 0);

  /* The golden frame happens to be the last frame. */
  if (all_refs[num].index_in_dpb ==
      va_frame->ref_frame_idx[GST_AV1_REF_GOLDEN_FRAME]) {
    num--;
    forward_ref_num--;
    gf_assigned = TRUE;
  }

  if (num >= 0 && forward_ref_num > 0) {
    va_frame->ref_frame_idx[GST_AV1_REF_LAST_FRAME] =
        all_refs[num].index_in_dpb;
  } else {
    /* At least one forward reference.
       Just let the last frame be the same as the golden frame. */
    g_assert (gf_assigned);
    va_frame->ref_frame_idx[GST_AV1_REF_LAST_FRAME] =
        va_frame->ref_frame_idx[GST_AV1_REF_GOLDEN_FRAME];
  }

  num--;
  forward_ref_num--;

  if (!gf_assigned) {
    if (forward_ref_num <= 0) {
      va_frame->ref_frame_idx[GST_AV1_REF_GOLDEN_FRAME] =
          va_frame->ref_frame_idx[GST_AV1_REF_LAST_FRAME];
    } else {
      /* The golden frame index is already found. */
      forward_ref_num--;
    }
  }

  if (num >= 0 && all_refs[num].index_in_dpb ==
      va_frame->ref_frame_idx[GST_AV1_REF_GOLDEN_FRAME])
    num--;

  if (num >= 0 && forward_ref_num > 0) {
    va_frame->ref_frame_idx[GST_AV1_REF_LAST2_FRAME] =
        all_refs[num].index_in_dpb;
  } else {
    va_frame->ref_frame_idx[GST_AV1_REF_LAST2_FRAME] =
        va_frame->ref_frame_idx[GST_AV1_REF_LAST_FRAME];
  }

  num--;
  forward_ref_num--;
  if (num >= 0 && all_refs[num].index_in_dpb ==
      va_frame->ref_frame_idx[GST_AV1_REF_GOLDEN_FRAME])
    num--;

  if (num >= 0 && forward_ref_num > 0) {
    va_frame->ref_frame_idx[GST_AV1_REF_LAST3_FRAME] =
        all_refs[num].index_in_dpb;
  } else {
    va_frame->ref_frame_idx[GST_AV1_REF_LAST3_FRAME] =
        va_frame->ref_frame_idx[GST_AV1_REF_LAST_FRAME];
  }

  /* Setting the backward refs */
  if (backward_num > 0 && self->gop.backward_ref_num > 0) {
    backward_ref_num = self->gop.backward_ref_num;

    g_assert (_enc_frame (all_refs[ref_num - 1].frame)->flags & FRAME_FLAG_ALT);

    va_frame->bidir_ref = TRUE;

    if (backward_num >= 3 && backward_ref_num >= 3) {
      /* Set the BWDREF to the nearest future frame, ALTREF2 to the next
         nearest future frame and ALTREF to the furthest future frame
         in the GF group. */
      va_frame->ref_frame_idx[GST_AV1_REF_ALTREF_FRAME] =
          all_refs[ref_num - 1].index_in_dpb;
      va_frame->ref_frame_idx[GST_AV1_REF_ALTREF2_FRAME] =
          all_refs[forward_num + 1].index_in_dpb;
      va_frame->ref_frame_idx[GST_AV1_REF_BWDREF_FRAME] =
          all_refs[forward_num].index_in_dpb;
    } else if (backward_num == 2 && backward_ref_num >= 2) {
      /* Set the BWDREF to the nearest future frame and ALTREF to the furthest
         future frame in the GF group. ALTREF2 is just set to GOLDEN. */
      va_frame->ref_frame_idx[GST_AV1_REF_ALTREF_FRAME] =
          all_refs[ref_num - 1].index_in_dpb;
      va_frame->ref_frame_idx[GST_AV1_REF_BWDREF_FRAME] =
          all_refs[forward_num].index_in_dpb;

      va_frame->ref_frame_idx[GST_AV1_REF_ALTREF2_FRAME] =
          va_frame->ref_frame_idx[GST_AV1_REF_ALTREF_FRAME];
    } else {
      /* Set the ALTREF to the nearest future frame. ALTREF2 and BWDREF
         are just set to GOLDEN. */
      va_frame->ref_frame_idx[GST_AV1_REF_ALTREF_FRAME] =
          all_refs[forward_num].index_in_dpb;

      va_frame->ref_frame_idx[GST_AV1_REF_ALTREF2_FRAME] =
          va_frame->ref_frame_idx[GST_AV1_REF_ALTREF_FRAME];
      va_frame->ref_frame_idx[GST_AV1_REF_BWDREF_FRAME] =
          va_frame->ref_frame_idx[GST_AV1_REF_ALTREF_FRAME];
    }
  } else {
    /* If no backward refs, BWDREF, ALTREF and ALTREF2 are set to GOLDEN. */
    va_frame->ref_frame_idx[GST_AV1_REF_ALTREF_FRAME] =
        va_frame->ref_frame_idx[GST_AV1_REF_GOLDEN_FRAME];
    va_frame->ref_frame_idx[GST_AV1_REF_ALTREF2_FRAME] =
        va_frame->ref_frame_idx[GST_AV1_REF_GOLDEN_FRAME];
    va_frame->ref_frame_idx[GST_AV1_REF_BWDREF_FRAME] =
        va_frame->ref_frame_idx[GST_AV1_REF_GOLDEN_FRAME];
  }

finish:
  _av1_print_frame_reference (self, frame);
  return TRUE;
}

static void
_av1_find_ref_to_update (GstVaBaseEnc * base, GstVideoCodecFrame * frame)
{
  GstVaAV1Enc *self = GST_VA_AV1_ENC (base);
  GstVaAV1EncFrame *va_frame = _enc_frame (frame);
  gint slot;
  gint lowest_slot;
  gint lowest_frame_num = MAX_KEY_FRAME_INTERVAL + 1;
  gint i;

  if (va_frame->type & FRAME_TYPE_REPEAT)
    return;

  if ((va_frame->flags & FRAME_FLAG_UPDATE_REF) == 0) {
    /* Key frame should always clean the reference list. */
    g_assert (va_frame->type != GST_AV1_KEY_FRAME);
    return;
  }

  va_frame->update_index = -1;

  /* key frame will clear the whole ref list, just use the 0 */
  if (va_frame->type == GST_AV1_KEY_FRAME) {
    va_frame->update_index = 0;
    return;
  }

  /* 1. Find an empty slot in the reference list.
     2. If the list is full, kick out the non GF frame with lowest
     frame num. GF frame should not be kicked out because we always
     set GOLDEN to GF frame.
     3. If still not find, we drop ourself. */
  lowest_frame_num = MAX_KEY_FRAME_INTERVAL + 1;
  slot = lowest_slot = -1;
  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
    GstVaAV1EncFrame *va_f;

    if (self->gop.ref_list[i] == NULL) {
      slot = i;
      break;
    }

    va_f = _enc_frame (self->gop.ref_list[i]);
    if (va_f->flags & FRAME_FLAG_GF)
      continue;

    if (va_f->frame_num > va_frame->frame_num)
      continue;

    if (va_f->frame_num < lowest_frame_num) {
      lowest_frame_num = va_f->frame_num;
      lowest_slot = i;
    }
  }

  if (slot < 0 && lowest_slot >= 0)
    slot = lowest_slot;

  if (slot >= 0)
    va_frame->update_index = slot;
}

static void
_av1_update_ref_list (GstVaBaseEnc * base, GstVideoCodecFrame * frame)
{
  GstVaAV1Enc *self = GST_VA_AV1_ENC (base);
  GstVaAV1EncFrame *va_frame = _enc_frame (frame);
  GstVideoCodecFrame *del_f;
  gint i;

  if (va_frame->type & FRAME_TYPE_REPEAT)
    return;

  /* key frame, clear the whole ref list. */
  if (va_frame->type == GST_AV1_KEY_FRAME) {
    g_assert (va_frame->update_index == 0);
    g_assert (va_frame->flags & FRAME_FLAG_UPDATE_REF);

    for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
      if (self->gop.ref_list[i] == NULL)
        continue;

      g_queue_remove (&base->ref_list, self->gop.ref_list[i]);
      gst_video_codec_frame_unref (self->gop.ref_list[i]);
      self->gop.ref_list[i] = NULL;
    }

    g_assert (g_queue_is_empty (&base->ref_list));
  }

  /* We drop ourself. */
  if (va_frame->update_index < 0) {
    GST_DEBUG_OBJECT (self, "Drop the non ref frame %d,"
        " reference list unchanged", va_frame->frame_num);
    return;
  }

  del_f = self->gop.ref_list[va_frame->update_index];

  g_queue_push_tail (&base->ref_list, gst_video_codec_frame_ref (frame));
  self->gop.ref_list[va_frame->update_index] = frame;

  _av1_print_ref_list_update (self, va_frame->update_index, del_f, frame);

  if (del_f) {
    g_queue_remove (&base->ref_list, del_f);
    gst_video_codec_frame_unref (del_f);
  }
}

static void
_av1_clear_frames_in_tu (GstVaAV1Enc * self)
{
  guint i;
  GstVaAV1EncFrame *frame_enc;

  for (i = 0; i < self->frames_in_tu_num; i++) {
    frame_enc = _enc_frame (self->frames_in_tu[i]);
    frame_enc->flags &= (~FRAME_FLAG_FRAME_IN_TU_CACHE);
  }

  memset (self->frames_in_tu, 0, sizeof (self->frames_in_tu));
  self->frames_in_tu_num = 0;
}

static void
gst_va_av1_enc_reset_state (GstVaBaseEnc * base)
{
  GstVaAV1Enc *self = GST_VA_AV1_ENC (base);

  GST_VA_BASE_ENC_CLASS (parent_class)->reset_state (base);

  GST_OBJECT_LOCK (self);
  self->rc.rc_ctrl_mode = self->prop.rc_ctrl;
  self->rc.target_usage = self->prop.target_usage;
  self->rc.base_qindex = self->prop.qp;
  self->rc.min_qindex = self->prop.min_qp;
  self->rc.max_qindex = self->prop.max_qp;
  self->rc.target_percentage = self->prop.target_percentage;
  self->rc.cpb_size = self->prop.cpb_size;
  self->rc.mbbrc = self->prop.mbbrc;

  self->gop.keyframe_interval = self->prop.keyframe_interval;
  self->gop.gf_group_size = self->prop.gf_group_size;
  self->gop.num_ref_frames = self->prop.num_ref_frames;
  self->gop.max_level = self->prop.max_hierarchical_level;
  self->partition.use_128x128_superblock = self->prop.use_128x128_superblock;
  self->partition.num_tile_cols = self->prop.num_tile_cols;
  self->partition.num_tile_rows = self->prop.num_tile_rows;
  self->partition.tile_groups = self->prop.tile_groups;
  GST_OBJECT_UNLOCK (self);

  self->packed_headers = 0;

  self->mi_rows = 0;
  self->mi_cols = 0;
  self->depth = 0;
  self->chrome = 0;
  self->level_idx = -1;
  self->level_str = NULL;
  self->tier = 0;
  self->cr = 0;
  self->last_pts = GST_CLOCK_TIME_NONE;
  self->last_dts = GST_CLOCK_TIME_NONE;

  self->features.enable_filter_intra = FALSE;
  self->features.enable_intra_edge_filter = FALSE;
  self->features.enable_interintra_compound = FALSE;
  self->features.enable_masked_compound = FALSE;
  self->features.enable_warped_motion = FALSE;
  self->features.enable_palette_mode = FALSE;
  self->features.enable_dual_filter = FALSE;
  self->features.enable_jnt_comp = FALSE;
  self->features.enable_ref_frame_mvs = FALSE;
  self->features.enable_superres = FALSE;
  self->features.enable_restoration = FALSE;
  self->features.allow_intrabc = FALSE;
  self->features.enable_segmentation = FALSE;
  self->features.enable_cdef = FALSE;
  self->features.interpolation_filter_support = 0;
  self->features.interpolation_type = 0;
  self->features.obu_size_bytes = 0;
  self->features.tx_mode_support = 0;
  self->features.cdef_channel_strength = FALSE;

  _av1_init_gf_group (&self->gop.current_group, &base->reorder_list);
  self->gop.last_keyframe = NULL;
  memset (self->gop.ref_list, 0, sizeof (self->gop.ref_list));
  self->gop.frame_num_since_kf = 0;
  self->gop.forward_only_ref_num = 0;
  self->gop.forward_ref_num = 0;
  self->gop.backward_ref_num = 0;
  self->gop.enable_order_hint = FALSE;

  self->partition.sb_rows = 0;
  self->partition.sb_cols = 0;
  self->partition.tile_size_bytes_minus_1 = 0;
  self->partition.tile_width_sb = 0;
  self->partition.tile_height_sb = 0;
  self->partition.uniform = TRUE;
  self->partition.max_tile_num = 0;
  self->partition.tile_cols_log2 = 0;
  self->partition.tile_rows_log2 = 0;

  self->rc.max_bitrate = 0;
  self->rc.target_bitrate = 0;
  self->rc.max_bitrate_bits = 0;
  self->rc.cpb_length_bits = 0;

  memset (self->frames_in_tu, 0, sizeof (self->frames_in_tu));
  self->frames_in_tu_num = 0;
  self->inside_tu = FALSE;

  memset (&self->sequence_hdr, 0, sizeof (self->sequence_hdr));
}

static gboolean
gst_va_av1_enc_flush (GstVideoEncoder * venc)
{
  GstVaAV1Enc *self = GST_VA_AV1_ENC (venc);
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);

  _av1_clear_frames_in_tu (self);
  self->inside_tu = FALSE;

  /* begin from an key frame after flush. */
  self->gop.frame_num_since_kf = 0;

  /* Parent's flush will release all frames for us. */
  _av1_init_gf_group (&self->gop.current_group, &base->reorder_list);
  self->gop.last_keyframe = NULL;
  memset (self->gop.ref_list, 0, sizeof (self->gop.ref_list));

  return GST_VIDEO_ENCODER_CLASS (parent_class)->flush (venc);
}

static guint
_av1_get_rtformat (GstVaAV1Enc * self, GstVideoFormat format,
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
    case VA_RT_FORMAT_YUV420_10:
      *depth = 10;
      *chrome = 1;
      break;
    case VA_RT_FORMAT_YUV444:
      *depth = 8;
      *chrome = 3;
      break;
    case VA_RT_FORMAT_YUV422_10:
      *depth = 10;
      *chrome = 2;
      break;
    default:
      chroma = 0;
      GST_ERROR_OBJECT (self, "Unsupported chroma for video format: %s",
          gst_video_format_to_string (format));
      break;
  }

  return chroma;
}

#define update_property(type, obj, old_val, new_val, prop_id)           \
  gst_va_base_enc_update_property_##type (obj, old_val, new_val, properties[prop_id])
#define update_property_uint(obj, old_val, new_val, prop_id)    \
  update_property (uint, obj, old_val, new_val, prop_id)
#define update_property_bool(obj, old_val, new_val, prop_id)    \
  update_property (bool, obj, old_val, new_val, prop_id)

static gboolean
_av1_decide_profile (GstVaAV1Enc * self, guint rt_format,
    guint depth, guint chrome)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  GstCaps *allowed_caps = NULL;
  guint num_structures, i;
  GstStructure *structure;
  const GValue *v_profile;
  GArray *candidates = NULL;
  VAProfile va_profile, ret_profile = VAProfileNone;

  candidates = g_array_new (TRUE, TRUE, sizeof (VAProfile));

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
        va_profile =
            gst_va_profile_from_name (AV1, g_value_get_string (v_profile));
        g_array_append_val (candidates, va_profile);
      } else if (GST_VALUE_HOLDS_LIST (v_profile)) {
        guint j;

        for (j = 0; j < gst_value_list_get_size (v_profile); j++) {
          const GValue *p = gst_value_list_get_value (v_profile, j);
          if (!p)
            continue;

          va_profile = gst_va_profile_from_name (AV1, g_value_get_string (p));
          g_array_append_val (candidates, va_profile);
        }
      }
    }
  }

  if (candidates->len == 0) {
    GST_ERROR_OBJECT (self, "No available profile in caps");
    goto out;
  }

  /* 6.4.1:
     seq_profile  Bit depth  Monochrome support  Chroma subsampling
     0            8 or 10    Yes                 YUV 4:2:0
     1            8 or 10    No                  YUV 4:4:4
     2            8 or 10    Yes                 YUV 4:2:2
     2            12         Yes                 YUV 4:2:0,YUV 4:2:2,YUV 4:4:4
   */
  /* We only support 0 and 1 profile now */
  if (chrome == 0 || chrome == 1) {
    va_profile = VAProfileAV1Profile0;
  } else if (chrome == 3) {
    va_profile = VAProfileAV1Profile1;
  } else {
    va_profile = VAProfileNone;
    GST_ERROR_OBJECT (self, "No suitable profile for chroma value %d", chrome);
    goto out;
  }

  for (i = 0; i < candidates->len; i++) {
    VAProfile p;

    p = g_array_index (candidates, VAProfile, i);
    if (!gst_va_encoder_has_profile (base->encoder, p))
      continue;

    if ((rt_format & gst_va_encoder_get_rtformat (base->encoder,
                p, GST_VA_BASE_ENC_ENTRYPOINT (base))) == 0)
      continue;

    if (p == va_profile) {
      ret_profile = p;
      goto out;
    }
  }

out:
  if (ret_profile != VAProfileNone)
    GST_INFO_OBJECT (self, "Decide the profile: %s",
        gst_va_profile_name (ret_profile));

  return ret_profile;
}

static gboolean
_av1_init_packed_headers (GstVaAV1Enc * self)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint32 packed_headers;
  guint32 desired_packed_headers = VA_ENC_PACKED_HEADER_SEQUENCE        /* Sequence Header */
      | VA_ENC_PACKED_HEADER_PICTURE    /* Frame Header */
      | VA_ENC_PACKED_HEADER_RAW_DATA;  /* Meta, TU, etc. */

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

static gboolean
_av1_generate_gop_structure (GstVaAV1Enc * self)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint32 list0, list1, listp_0;

  /* If not set, generate a key frame every 2 second */
  if (self->gop.keyframe_interval == 0) {
    self->gop.keyframe_interval = (2 * GST_VIDEO_INFO_FPS_N (&base->in_info)
        + GST_VIDEO_INFO_FPS_D (&base->in_info) - 1) /
        GST_VIDEO_INFO_FPS_D (&base->in_info);
  }

  if (self->gop.keyframe_interval > MAX_KEY_FRAME_INTERVAL)
    self->gop.keyframe_interval = MAX_KEY_FRAME_INTERVAL;

  if (self->gop.gf_group_size >= self->gop.keyframe_interval)
    self->gop.gf_group_size = self->gop.keyframe_interval - 1;

  if (!gst_va_encoder_get_max_num_reference (base->encoder, base->profile,
          GST_VA_BASE_ENC_ENTRYPOINT (base), &list0, &list1)) {
    GST_INFO_OBJECT (self, "Failed to get the max num reference");
    list0 = 1;
    list1 = 0;
    listp_0 = list0;
  }

  listp_0 = list0;

  /* At most, 4 forward refs */
  if (list0 > 4)
    list0 = 4;
  if (listp_0 > 4)
    listp_0 = 4;

  /* At most, 3 backward refs */
  if (list1 > 3)
    list1 = 3;

  /* No more backward refs than forward refs. */
  if (list1 > list0)
    list1 = list0;

  /* Do not let P frames have more refs than B frames. */
  if (listp_0 > list0 + list1)
    listp_0 = list0 + list1;
  /* B frame should not have more forward refs than P frame */
  if (listp_0 != 0 && list0 > listp_0)
    list0 = listp_0;

  /* Only I/P mode is needed */
  if (self->gop.max_level < 2 || self->gop.gf_group_size < 3) {
    list1 = 0;
    list0 = listp_0;
  }

  if (self->gop.num_ref_frames == 0) {
    list0 = 0;
    listp_0 = 0;
    list1 = 0;
    self->gop.num_ref_frames = list0 + list1;
    GST_INFO_OBJECT (self, "No reference for each frame, intra only stream");
  } else if (self->gop.num_ref_frames <= 2) {
    list0 = MIN (self->gop.num_ref_frames, list0);
    listp_0 = list0;
    list1 = 0;
    self->gop.num_ref_frames = list0 + list1;
    GST_INFO_OBJECT (self, "Only %d reference frames, disable backward ref",
        self->gop.num_ref_frames);
  } else {
    if (self->gop.num_ref_frames > list0 + list1) {
      self->gop.num_ref_frames = list0 + list1;
      GST_INFO_OBJECT (self, "Lowering the number of reference frames to %d "
          "because of the reference number limit", self->gop.num_ref_frames);
    } else if (self->gop.num_ref_frames < list0 + list1) {
      guint l0 = 0;
      guint l1 = 0;

      g_assert (list0 > 0);
      g_assert (list0 >= list1);

      while (list0 > 0 || list1 > 0) {
        if (list0 > 0) {
          l0++;
          list0--;
        }
        if (l0 + l1 >= self->gop.num_ref_frames)
          break;

        if (list1 > 0) {
          l1++;
          list1--;
        }

        if (l0 + l1 >= self->gop.num_ref_frames)
          break;
      }

      list0 = l0;
      list1 = l1;
      listp_0 = MIN (l0 + l1, listp_0);

      self->gop.num_ref_frames = list0 + list1;
    }
  }

  self->gop.forward_only_ref_num = listp_0;
  self->gop.forward_ref_num = list0;
  self->gop.backward_ref_num = list1;

  if (self->gop.num_ref_frames > 0) {
    self->gop.enable_order_hint = TRUE;
  } else {
    self->gop.enable_order_hint = FALSE;
  }

  GST_INFO_OBJECT (self, "key frame interval %d, golden frame group size %d,"
      " max hierarchical level %d, reference num %d, forward_only_ref_num %d,"
      " forward ref num %d, backward ref num %d, order hint is %d",
      self->gop.keyframe_interval, self->gop.gf_group_size, self->gop.max_level,
      self->gop.num_ref_frames, self->gop.forward_only_ref_num,
      self->gop.forward_ref_num, self->gop.backward_ref_num,
      self->gop.enable_order_hint);

  update_property_uint (base, &self->prop.keyframe_interval,
      self->gop.keyframe_interval, PROP_KEYFRAME_INT);
  update_property_uint (base, &self->prop.gf_group_size,
      self->gop.gf_group_size, PROP_GOLDEN_GROUP_SIZE);
  update_property_uint (base, &self->prop.num_ref_frames,
      self->gop.num_ref_frames, PROP_NUM_REF_FRAMES);
  update_property_uint (base, &self->prop.max_hierarchical_level,
      self->gop.max_level, PROP_HIERARCHICAL_LEVEL);

  _av1_init_gf_group (&self->gop.current_group, &base->reorder_list);

  return TRUE;
}

/* 5.9.16. Tile size calculation function */
static gint
_av1_tile_log2 (gint blkSize, gint target)
{
  gint k;

  for (k = 0; (blkSize << k) < target; k++);

  return k;
}

static gboolean
_av1_setup_tile_partition (GstVaAV1Enc * self)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint sb_shift, sb_size;
  guint max_tile_width_sb, max_tile_area_sb;
  guint min_log2_tile_cols, max_log2_tile_cols,
      max_log2_tile_rows, min_log2_tiles;

  /* 5.9.15. Tile info syntax */
  self->partition.sb_cols = self->partition.use_128x128_superblock ?
      ((self->mi_cols + 31) >> 5) : ((self->mi_cols + 15) >> 4);
  self->partition.sb_rows = self->partition.use_128x128_superblock ?
      ((self->mi_rows + 31) >> 5) : ((self->mi_rows + 15) >> 4);

  sb_shift = self->partition.use_128x128_superblock ? 5 : 4;
  sb_size = sb_shift + 2;
  max_tile_width_sb = GST_AV1_MAX_TILE_WIDTH >> sb_size;
  max_tile_area_sb = GST_AV1_MAX_TILE_AREA >> (2 * sb_size);

  min_log2_tile_cols =
      _av1_tile_log2 (max_tile_width_sb, self->partition.sb_cols);
  max_log2_tile_cols =
      _av1_tile_log2 (1, MIN (self->partition.sb_cols, GST_AV1_MAX_TILE_COLS));
  max_log2_tile_rows =
      _av1_tile_log2 (1, MIN (self->partition.sb_rows, GST_AV1_MAX_TILE_ROWS));
  min_log2_tiles = MAX (min_log2_tile_cols, _av1_tile_log2 (max_tile_area_sb,
          self->partition.sb_rows * self->partition.sb_cols));

  if (self->partition.max_tile_num < (1 << min_log2_tiles)) {
    GST_ERROR_OBJECT (self, "HW only support %d tiles, less than the min"
        " required tile number %d", self->partition.max_tile_num,
        (1 << min_log2_tiles));
    return FALSE;
  }

  if (self->partition.num_tile_cols * self->partition.num_tile_rows >
      self->partition.max_tile_num) {
    GST_ERROR_OBJECT (self, "HW only support %d tiles, less than the"
        " total tile number %dx%d = %d we set", self->partition.max_tile_num,
        self->partition.num_tile_cols, self->partition.num_tile_rows,
        self->partition.num_tile_cols * self->partition.num_tile_rows);
    return FALSE;
  }

  self->partition.tile_cols_log2 =
      _av1_tile_log2 (1, self->partition.num_tile_cols);
  if (self->partition.tile_cols_log2 < min_log2_tile_cols)
    self->partition.tile_cols_log2 = min_log2_tile_cols;
  if (self->partition.tile_cols_log2 > max_log2_tile_cols)
    self->partition.tile_cols_log2 = max_log2_tile_cols;

  self->partition.tile_rows_log2 =
      _av1_tile_log2 (1, self->partition.num_tile_rows);
  if (self->partition.tile_rows_log2 > max_log2_tile_rows)
    self->partition.tile_rows_log2 = max_log2_tile_rows;
  if (self->partition.tile_cols_log2 + self->partition.tile_rows_log2 <
      min_log2_tiles)
    self->partition.tile_rows_log2 =
        min_log2_tiles - self->partition.tile_cols_log2;

  /* Only support uniform now */
  self->partition.uniform = TRUE;
  self->partition.tile_width_sb = (self->partition.sb_cols +
      (1 << self->partition.tile_cols_log2) - 1)
      >> self->partition.tile_cols_log2;
  self->partition.tile_height_sb = (self->partition.sb_rows +
      (1 << self->partition.tile_rows_log2) - 1)
      >> self->partition.tile_rows_log2;

  self->partition.num_tile_cols = (self->partition.sb_cols +
      self->partition.tile_width_sb - 1) / self->partition.tile_width_sb;
  self->partition.num_tile_rows = (self->partition.sb_rows +
      self->partition.tile_height_sb - 1) / self->partition.tile_height_sb;

  /* At least one tile for each tile group. */
  if (self->partition.tile_groups >
      self->partition.num_tile_cols * self->partition.num_tile_rows)
    self->partition.tile_groups =
        self->partition.num_tile_cols * self->partition.num_tile_rows;

  update_property_uint (base, &self->prop.num_tile_cols,
      self->partition.num_tile_cols, PROP_NUM_TILE_COLS);
  update_property_uint (base, &self->prop.num_tile_rows,
      self->partition.num_tile_rows, PROP_NUM_TILE_ROWS);
  update_property_uint (base, &self->prop.tile_groups,
      self->partition.tile_groups, PROP_TILE_GROUPS);

  GST_INFO_OBJECT (self, "Tile info: uniform = %d, num_tile_cols = %d, "
      "num_tile_rows = %d, tile_cols_log2 = %d, tile_rows_log2 = %d, "
      "tile_width_sb = %d, tile_height_sb = %d, tile_groups = %d",
      self->partition.uniform, self->partition.num_tile_cols,
      self->partition.num_tile_rows, self->partition.tile_cols_log2,
      self->partition.tile_rows_log2, self->partition.tile_width_sb,
      self->partition.tile_height_sb, self->partition.tile_groups);

  return TRUE;
}

/* We need to decide the profile and entrypoint before call this.
   It applies the optimized features provided by the va driver. */
static void
_av1_setup_encoding_features (GstVaAV1Enc * self)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  VAStatus status;
  VAConfigAttrib attrib;

  attrib.type = VAConfigAttribEncAV1;
  attrib.value = 0;
  status = vaGetConfigAttributes (gst_va_display_get_va_dpy (base->display),
      base->profile, GST_VA_BASE_ENC_ENTRYPOINT (base), &attrib, 1);
  if (status != VA_STATUS_SUCCESS || attrib.value == VA_ATTRIB_NOT_SUPPORTED) {
    if (status != VA_STATUS_SUCCESS) {
      GST_INFO_OBJECT (self, "Failed to query encoding features: %s",
          vaErrorStr (status));
    } else {
      GST_INFO_OBJECT (self, "Driver does not support query encoding features");
    }

    GST_INFO_OBJECT (self, "Use default values for AV1 features");

    self->partition.use_128x128_superblock = FALSE;
    GST_INFO_OBJECT (self, "128x128 superblock query not supported,"
        " just disable it");

    self->features.enable_filter_intra = FALSE;
    self->features.enable_intra_edge_filter = FALSE;
    self->features.enable_interintra_compound = FALSE;
    self->features.enable_masked_compound = FALSE;
    self->features.enable_warped_motion = FALSE;
    self->features.enable_palette_mode = FALSE;
    self->features.enable_dual_filter = FALSE;
    self->features.enable_jnt_comp = FALSE;
    self->features.enable_ref_frame_mvs = FALSE;
    self->features.enable_superres = FALSE;
    self->features.enable_restoration = FALSE;
    self->features.allow_intrabc = FALSE;
    self->features.enable_cdef = FALSE;
    self->features.cdef_channel_strength = FALSE;
  } else {
    VAConfigAttribValEncAV1 features;

    features.value = attrib.value;

    if (self->partition.use_128x128_superblock
        && (features.bits.support_128x128_superblock == 0)) {
      GST_INFO_OBJECT (self, "128x128 superblock is not supported.");
      self->partition.use_128x128_superblock = FALSE;
    }

    self->features.enable_filter_intra =
        (features.bits.support_filter_intra != 0);
    self->features.enable_intra_edge_filter =
        (features.bits.support_intra_edge_filter != 0);
    self->features.enable_interintra_compound =
        (features.bits.support_interintra_compound != 0);
    self->features.enable_masked_compound =
        (features.bits.support_masked_compound != 0);
    /* not enable it now. */
    self->features.enable_warped_motion = FALSE;
    // (features.bits.support_warped_motion != 0);
    self->features.enable_palette_mode =
        (features.bits.support_palette_mode != 0);
    self->features.enable_dual_filter =
        (features.bits.support_dual_filter != 0);
    self->features.enable_jnt_comp = (features.bits.support_jnt_comp != 0);
    self->features.enable_ref_frame_mvs =
        (features.bits.support_ref_frame_mvs != 0);
    /* not enable it now. */
    self->features.enable_superres = FALSE;
    self->features.enable_restoration = FALSE;
    // (features.bits.support_restoration != 0);
    /* not enable it now. */
    self->features.allow_intrabc = FALSE;
    self->features.enable_cdef = TRUE;
    self->features.cdef_channel_strength =
        (features.bits.support_cdef_channel_strength != 0);
  }

  update_property_bool (base, &self->prop.use_128x128_superblock,
      self->partition.use_128x128_superblock, PROP_128X128_SUPERBLOCK);

  attrib.type = VAConfigAttribEncAV1Ext1;
  attrib.value = 0;
  status = vaGetConfigAttributes (gst_va_display_get_va_dpy (base->display),
      base->profile, GST_VA_BASE_ENC_ENTRYPOINT (base), &attrib, 1);
  if (status != VA_STATUS_SUCCESS || attrib.value == VA_ATTRIB_NOT_SUPPORTED) {
    if (status != VA_STATUS_SUCCESS) {
      GST_INFO_OBJECT (self, "Failed to query encoding feature ext1: %s",
          vaErrorStr (status));
    } else {
      GST_INFO_OBJECT (self, "Driver does not support query encoding"
          " feature ext1");
    }

    GST_INFO_OBJECT (self, "Use default values for AV1 feature ext1");

    /* Only EIGHTTAP */
    self->features.interpolation_filter_support =
        1 << GST_AV1_INTERPOLATION_FILTER_EIGHTTAP;
    self->features.interpolation_type = GST_AV1_INTERPOLATION_FILTER_EIGHTTAP;

    self->features.enable_segmentation = FALSE;
  } else {
    VAConfigAttribValEncAV1Ext1 features_ext1;
    guint i;

    features_ext1.value = attrib.value;

    self->features.interpolation_filter_support =
        (features_ext1.bits.interpolation_filter & 0x1f);
    if (self->features.interpolation_filter_support == 0) {
      GST_INFO_OBJECT (self, "No interpolation filter support,"
          " just assume it supports EIGHTTAP type");
      self->features.interpolation_filter_support =
          1 << GST_AV1_INTERPOLATION_FILTER_EIGHTTAP;
    }

    for (i = 0; i < 5; i++) {
      if (self->features.interpolation_filter_support & (1 << i)) {
        self->features.interpolation_type = i;
        break;
      }
    }

    /* not enable segmentation now. */
    self->features.enable_segmentation = FALSE;
  }

  attrib.type = VAConfigAttribEncAV1Ext2;
  attrib.value = 0;
  status = vaGetConfigAttributes (gst_va_display_get_va_dpy (base->display),
      base->profile, GST_VA_BASE_ENC_ENTRYPOINT (base), &attrib, 1);
  if (status != VA_STATUS_SUCCESS || attrib.value == VA_ATTRIB_NOT_SUPPORTED) {
    if (status != VA_STATUS_SUCCESS) {
      GST_INFO_OBJECT (self, "Failed to query encoding feature ext2: %s",
          vaErrorStr (status));
    } else {
      GST_INFO_OBJECT (self, "Driver does not support query encoding"
          " feature ext2");
    }

    GST_INFO_OBJECT (self, "Use default values for AV1 feature ext2");

    self->partition.tile_size_bytes_minus_1 = 3;
    self->features.obu_size_bytes = 4;
    self->features.tx_mode_support = GST_AV1_TX_MODE_LARGEST;
    self->partition.max_tile_num = 1;
  } else {
    VAConfigAttribValEncAV1Ext2 features_ext2;

    features_ext2.value = attrib.value;

    self->partition.tile_size_bytes_minus_1 =
        features_ext2.bits.tile_size_bytes_minus1;
    self->features.obu_size_bytes = 4;
    /* features_ext2.bits.obu_size_bytes_minus1 + 1; */

    self->features.tx_mode_support = features_ext2.bits.tx_mode_support;
    if (!(self->features.tx_mode_support &
            (1 << GST_AV1_TX_MODE_LARGEST | 1 << GST_AV1_TX_MODE_SELECT))) {
      GST_WARNING_OBJECT (self, "query tx_mode_support get invalid"
          " value 0x%x, set to support TX_MODE_LARGEST",
          self->features.tx_mode_support);
      self->features.tx_mode_support = 1 << GST_AV1_TX_MODE_LARGEST;
    }

    self->partition.max_tile_num = features_ext2.bits.max_tile_num_minus1 + 1;
  }

  GST_INFO_OBJECT (self, "Setting the features: use_128x128_superblock = %d,"
      " enable_filter_intra = %d, enable_intra_edge_filter = %d,"
      " enable_interintra_compound = %d, enable_masked_compound = %d,"
      " enable_warped_motion = %d, enable_palette_mode = %d,"
      " enable_dual_filter = %d, enable_jnt_comp = %d,"
      " enable_ref_frame_mvs = %d, enable_superres = %d,"
      " enable_restoration = %d, allow_intrabc = %d,"
      " enable_cdef = %d, cdef_channel_strength = %d,"
      " interpolation_filter_support = %d,"
      " interpolation_type = %d, enable_segmentation = %d,"
      " tile_size_bytes_minus_1 = %d, obu_size_bytes = %d,"
      " tx_mode_support = 0x%x, max_tile_num = %d",
      self->partition.use_128x128_superblock,
      self->features.enable_filter_intra,
      self->features.enable_intra_edge_filter,
      self->features.enable_interintra_compound,
      self->features.enable_masked_compound,
      self->features.enable_warped_motion, self->features.enable_palette_mode,
      self->features.enable_dual_filter, self->features.enable_jnt_comp,
      self->features.enable_ref_frame_mvs, self->features.enable_superres,
      self->features.enable_restoration, self->features.allow_intrabc,
      self->features.enable_cdef, self->features.cdef_channel_strength,
      self->features.interpolation_filter_support,
      self->features.interpolation_type, self->features.enable_segmentation,
      self->partition.tile_size_bytes_minus_1, self->features.obu_size_bytes,
      self->features.tx_mode_support, self->partition.max_tile_num);
}

static void
_av1_calculate_coded_size (GstVaAV1Enc * self)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint un_compressed_size /* UnCompressedSize */ ;
  guint pic_size_profile_factor /* PicSizeProfileFactor */ ;

  /* Annex A: */
  switch (base->profile) {
    case VAProfileAV1Profile0:
      pic_size_profile_factor = 15;
      break;
    case VAProfileAV1Profile1:
      pic_size_profile_factor = 30;
      break;
    default:
      pic_size_profile_factor = 36;
      break;
  }

  un_compressed_size =
      ((base->width * base->height * pic_size_profile_factor) >> 3);

  /* FIXME: Using only a rough approximation for bitstream headers.
     We do not calculate SpeedAdj and do not consider still_picture. */
  base->codedbuf_size = un_compressed_size / self->cr;

  GST_INFO_OBJECT (self, "Calculate codedbuf size: %u", base->codedbuf_size);
}

/* Normalizes bitrate (and CPB size) for HRD conformance */
static void
_av1_calculate_bitrate_hrd (GstVaAV1Enc * self)
{
  guint bitrate_bits, cpb_bits_size;

  bitrate_bits = self->rc.max_bitrate * 1000;
  GST_DEBUG_OBJECT (self, "Max bitrate: %u bits/sec", bitrate_bits);
  self->rc.max_bitrate_bits = bitrate_bits;

  bitrate_bits = self->rc.target_bitrate * 1000;
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

  cpb_bits_size = self->rc.cpb_size * 1000;

  GST_DEBUG_OBJECT (self, "HRD CPB size: %u bits", cpb_bits_size);
  self->rc.cpb_length_bits = cpb_bits_size;
}

/* Estimates a good enough bitrate if none was supplied */
static gboolean
_av1_ensure_rate_control (GstVaAV1Enc * self)
{
  /* User can specify the properties of: "bitrate", "target-percentage",
   * "max-qp", "min-qp", "qp", "mbbrc", "cpb-size", "rate-control" and
   * "target-usage" to control the RC behavior.
   *
   * "target-usage" is different from the others, it controls the encoding
   * speed and quality, while the others control encoding bit rate and
   * quality. The lower value has better quality(maybe bigger MV search
   * range) but slower speed, the higher value has faster speed but lower
   * quality.
   *
   * The possible composition to control the bit rate and quality:
   *
   * 1. CQP mode: "rate-control=cqp", then "qp"(the qindex in AV1) specify
   *    the QP of frames(within the "max-qp" and "min-qp" range). The QP
   *    will not change during the whole stream. Other properties related
   *    to rate control are ignored.
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
  guint32 rc_ctrl, rc_mode, quality_level;

  quality_level = gst_va_encoder_get_quality_level (base->encoder,
      base->profile, GST_VA_BASE_ENC_ENTRYPOINT (base));
  if (self->rc.target_usage > quality_level) {
    GST_INFO_OBJECT (self, "User setting target-usage: %d is not supported, "
        "fallback to %d", self->rc.target_usage, quality_level);
    self->rc.target_usage = quality_level;

    update_property_uint (base, &self->prop.target_usage, self->rc.target_usage,
        PROP_TARGET_USAGE);
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

      update_property_uint (base, &self->prop.rc_ctrl, self->rc.rc_ctrl_mode,
          PROP_RATE_CONTROL);
    }
  } else {
    self->rc.rc_ctrl_mode = VA_RC_NONE;
  }

  if (self->rc.min_qindex > self->rc.max_qindex) {
    GST_INFO_OBJECT (self, "The min_qindex %d is bigger than the max_qindex"
        " %d, set it to the max_qindex", self->rc.min_qindex,
        self->rc.max_qindex);
    self->rc.min_qindex = self->rc.max_qindex;

    update_property_uint (base, &self->prop.min_qp, self->rc.min_qindex,
        PROP_MIN_QP);
  }

  /* Make the qp in the valid range */
  if (self->rc.base_qindex < self->rc.min_qindex) {
    if (self->rc.base_qindex != DEFAULT_BASE_QINDEX)
      GST_INFO_OBJECT (self, "The base_qindex %d is smaller than the"
          " min_qindex %d, set it to the min_qindex", self->rc.base_qindex,
          self->rc.min_qindex);
    self->rc.base_qindex = self->rc.min_qindex;
  }
  if (self->rc.base_qindex > self->rc.max_qindex) {
    if (self->rc.base_qindex != DEFAULT_BASE_QINDEX)
      GST_INFO_OBJECT (self, "The base_qindex %d is bigger than the"
          " max_qindex %d, set it to the max_qindex", self->rc.base_qindex,
          self->rc.max_qindex);
    self->rc.base_qindex = self->rc.max_qindex;
  }

  GST_OBJECT_LOCK (self);
  bitrate = self->prop.bitrate;
  GST_OBJECT_UNLOCK (self);

  /* Calculate a bitrate if it is not set. */
  if ((self->rc.rc_ctrl_mode == VA_RC_CBR || self->rc.rc_ctrl_mode == VA_RC_VBR
          || self->rc.rc_ctrl_mode == VA_RC_VCM) && bitrate == 0) {
    /* FIXME: Provide better estimation. */
    /* Choose the max value of all levels' MainCR which is 8, and x2 for
       conservative calculation. So just using a 1/16 compression ratio,
       12 bits per pixel for 4:2:0, 16 bits per pixel for 4:2:2 and 24 bits
       per pixel for 4:4:4. Also the depth should be considered. */
    guint64 factor;
    guint depth = 8, chrome = 1;
    guint bits_per_pix;

    if (!_av1_get_rtformat (self,
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

    factor = (guint64) base->width * base->height * bits_per_pix / 16;
    bitrate = gst_util_uint64_scale (factor,
        GST_VIDEO_INFO_FPS_N (&base->in_info),
        GST_VIDEO_INFO_FPS_D (&base->in_info)) / 1000;

    GST_INFO_OBJECT (self, "target bitrate computed to %u kbps", bitrate);

    self->prop.bitrate = bitrate;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BITRATE]);
  }

  /* Adjust the setting based on RC mode. */
  switch (self->rc.rc_ctrl_mode) {
    case VA_RC_NONE:
    case VA_RC_CQP:
      self->rc.max_bitrate = 0;
      self->rc.target_bitrate = 0;
      self->rc.target_percentage = 0;
      self->rc.cpb_size = 0;
      self->rc.mbbrc = 0;
      break;
    case VA_RC_CBR:
      self->rc.max_bitrate = bitrate;
      self->rc.target_bitrate = bitrate;
      self->rc.target_percentage = 100;
      self->rc.base_qindex = DEFAULT_BASE_QINDEX;
      break;
    case VA_RC_VBR:
      g_assert (self->rc.target_percentage >= 10);
      self->rc.max_bitrate = (guint) gst_util_uint64_scale_int (bitrate,
          100, self->rc.target_percentage);
      self->rc.target_bitrate = bitrate;
      self->rc.base_qindex = DEFAULT_BASE_QINDEX;
      break;
    case VA_RC_VCM:
      self->rc.max_bitrate = bitrate;
      self->rc.target_bitrate = bitrate;
      self->rc.target_percentage = 0;
      self->rc.base_qindex = DEFAULT_BASE_QINDEX;
      self->rc.cpb_size = 0;

      if (self->gop.max_level > 1) {
        GST_INFO_OBJECT (self, "VCM mode does not reorder frames");
        self->gop.max_level = 1;
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
    _av1_calculate_bitrate_hrd (self);

  /* notifications */
  update_property_uint (base, &self->prop.cpb_size, self->rc.cpb_size,
      PROP_CPB_SIZE);
  update_property_uint (base, &self->prop.target_percentage,
      self->rc.target_percentage, PROP_TARGET_PERCENTAGE);
  update_property_uint (base, &self->prop.qp, self->rc.base_qindex, PROP_QP);
  update_property_uint (base, &self->prop.mbbrc, self->rc.mbbrc, PROP_MBBRC);

  return TRUE;
}

static gboolean
gst_va_av1_enc_reconfig (GstVaBaseEnc * base)
{
  GstVaBaseEncClass *klass = GST_VA_BASE_ENC_GET_CLASS (base);
  GstVideoEncoder *venc = GST_VIDEO_ENCODER (base);
  GstVaAV1Enc *self = GST_VA_AV1_ENC (base);
  GstCaps *out_caps, *reconf_caps = NULL;
  GstVideoCodecState *output_state;
  GstVideoFormat format, reconf_format = GST_VIDEO_FORMAT_UNKNOWN;
  VAProfile profile;
  gboolean do_renegotiation = TRUE, do_reopen, need_negotiation;
  guint max_ref_frames, max_surfaces = 0,
      rt_format, depth = 0, chrome = 0, codedbuf_size;
  gint width, height;

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

  rt_format = _av1_get_rtformat (self, format, &depth, &chrome);
  if (!rt_format) {
    GST_ERROR_OBJECT (self, "unrecognized input format.");
    return FALSE;
  }

  profile = _av1_decide_profile (self, rt_format, depth, chrome);
  if (profile == VAProfileNone)
    return FALSE;

  /* first check */
  do_reopen = !(base->profile == profile && base->rt_format == rt_format
      && format == reconf_format && width == base->width
      && height == base->height && self->prop.rc_ctrl == self->rc.rc_ctrl_mode
      && depth == self->depth && chrome == self->chrome);

  if (do_reopen && gst_va_encoder_is_open (base->encoder))
    gst_va_encoder_close (base->encoder);

  gst_va_base_enc_reset_state (base);

  base->profile = profile;
  base->rt_format = rt_format;
  self->depth = depth;
  self->chrome = chrome;
  base->width = width;
  base->height = height;
  self->mi_cols = 2 * ((base->width + 7) >> 3);
  self->mi_rows = 2 * ((base->height + 7) >> 3);

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

  GST_DEBUG_OBJECT (self, "resolution:%dx%d, Mi size: %dx%d, "
      "frame duration is %" GST_TIME_FORMAT, base->width, base->height,
      self->mi_cols, self->mi_rows, GST_TIME_ARGS (base->frame_duration));

  if (!_av1_ensure_rate_control (self))
    return FALSE;

  if (!_av1_calculate_level_and_tier (self))
    return FALSE;

  if (!_av1_init_packed_headers (self))
    return FALSE;

  _av1_setup_encoding_features (self);

  if (!_av1_generate_gop_structure (self))
    return FALSE;

  if (!_av1_setup_tile_partition (self))
    return FALSE;

  _av1_calculate_coded_size (self);

  max_ref_frames = GST_AV1_NUM_REF_FRAMES + 3 /* scratch frames */ ;

  /* second check after calculations */
  do_reopen |=
      !(max_ref_frames == max_surfaces && codedbuf_size == base->codedbuf_size);
  if (do_reopen && gst_va_encoder_is_open (base->encoder))
    gst_va_encoder_close (base->encoder);

  if (!gst_va_encoder_is_open (base->encoder)
      && !gst_va_encoder_open (base->encoder, base->profile,
          GST_VIDEO_INFO_FORMAT (&base->in_info), base->rt_format,
          base->width, base->height, base->codedbuf_size, max_ref_frames,
          self->rc.rc_ctrl_mode, self->packed_headers)) {
    GST_ERROR_OBJECT (self, "Failed to open the VA encoder.");
    return FALSE;
  }

  /* Add some tags */
  gst_va_base_enc_add_codec_tag (base, "AV1");

  out_caps = gst_va_profile_caps (base->profile, klass->entrypoint);
  g_assert (out_caps);
  out_caps = gst_caps_fixate (out_caps);

  if (self->level_str)
    gst_caps_set_simple (out_caps, "level", G_TYPE_STRING, self->level_str,
        NULL);

  gst_caps_set_simple (out_caps, "width", G_TYPE_INT, base->width,
      "height", G_TYPE_INT, base->height, "alignment", G_TYPE_STRING, "tu",
      "stream-format", G_TYPE_STRING, "obu-stream", NULL);

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

static inline void
_av1_fill_sequence_param (GstVaAV1Enc * self,
    VAEncSequenceParameterBufferAV1 * sequence)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint8 seq_profile;
  guint8 order_hint_bits_minus_1;

  if (base->profile == VAProfileAV1Profile0) {
    seq_profile = 0;
  } else if (base->profile == VAProfileAV1Profile1) {
    seq_profile = 1;
  } else {
    GST_ERROR_OBJECT (self, "VA profile %d not supported", base->profile);
    g_assert_not_reached ();
    return;
  }

  order_hint_bits_minus_1 = _av1_helper_msb (self->gop.keyframe_interval);
  if (order_hint_bits_minus_1 > MAX_ORDER_HINT_BITS_MINUS_1)
    order_hint_bits_minus_1 = MAX_ORDER_HINT_BITS_MINUS_1;

  /* *INDENT-OFF* */
  *sequence = (VAEncSequenceParameterBufferAV1) {
    .seq_profile = seq_profile,
    .seq_level_idx = self->level_idx,
    .seq_tier = self->tier,
    .intra_period = self->gop.num_ref_frames == 0 ? 1 :
        self->gop.keyframe_interval,
    .ip_period = self->gop.backward_ref_num == 0 ? 1 :
        self->gop.gf_group_size,
    .bits_per_second = self->rc.target_bitrate_bits,
    .seq_fields.bits = {
      .still_picture = 0,
      .use_128x128_superblock = self->partition.use_128x128_superblock,
      .enable_filter_intra = self->features.enable_filter_intra,
      .enable_intra_edge_filter = self->features.enable_intra_edge_filter,
      .enable_interintra_compound = self->features.enable_interintra_compound,
      .enable_masked_compound = self->features.enable_masked_compound,
      .enable_warped_motion = self->features.enable_warped_motion,
      .enable_dual_filter = self->features.enable_dual_filter,
      .enable_order_hint = self->gop.enable_order_hint,
      .enable_jnt_comp = self->features.enable_jnt_comp,
      .enable_ref_frame_mvs = self->features.enable_ref_frame_mvs,
      .enable_superres = self->features.enable_superres,
      .enable_cdef = self->features.enable_cdef,
      .enable_restoration = self->features.enable_restoration,
      .bit_depth_minus8 = self->depth - 8,
      .subsampling_x = (self->chrome != 3),
      .subsampling_y = (self->chrome != 3 && self->chrome != 2),
    },
    .order_hint_bits_minus_1 = order_hint_bits_minus_1,
  };
  /* *INDENT-ON* */
}

static void
_av1_fill_sequence_header (GstVaAV1Enc * self,
    VAEncSequenceParameterBufferAV1 * seq_param)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);

  /* *INDENT-OFF* */
  self->sequence_hdr = (GstAV1SequenceHeaderOBU) {
    .seq_profile = seq_param->seq_profile,
    .still_picture = 0,
    .num_planes = (self->chrome == 0 ? 1 : 3),
    .reduced_still_picture_header = 0,
    .timing_info_present_flag = 0,
    .decoder_model_info_present_flag = 0,
    .initial_display_delay_present_flag = 0,
    .operating_points_cnt_minus_1 = 0,
    .operating_points = {
      {
        .seq_level_idx = seq_param->seq_level_idx,
        .seq_tier = seq_param->seq_tier,
        .idc = 0,
      },
    },
    .frame_width_bits_minus_1 = _av1_helper_msb (base->width),
    .frame_height_bits_minus_1 = _av1_helper_msb (base->height),
    .max_frame_width_minus_1 = base->width - 1,
    .max_frame_height_minus_1 = base->height - 1,
    .frame_id_numbers_present_flag = 0,
    .use_128x128_superblock = seq_param->seq_fields.bits.use_128x128_superblock,
    .enable_filter_intra = seq_param->seq_fields.bits.enable_filter_intra,
    .enable_intra_edge_filter = seq_param->seq_fields.bits.enable_intra_edge_filter,
    .enable_interintra_compound = seq_param->seq_fields.bits.enable_interintra_compound,
    .enable_masked_compound = seq_param->seq_fields.bits.enable_masked_compound,
    .enable_warped_motion = seq_param->seq_fields.bits.enable_warped_motion,
    .enable_dual_filter = seq_param->seq_fields.bits.enable_dual_filter,
    .enable_order_hint = seq_param->seq_fields.bits.enable_order_hint,
    .enable_jnt_comp = seq_param->seq_fields.bits.enable_jnt_comp,
    .enable_ref_frame_mvs = seq_param->seq_fields.bits.enable_ref_frame_mvs,
    .seq_choose_screen_content_tools = 0,
    .order_hint_bits_minus_1 = seq_param->order_hint_bits_minus_1,
    .enable_superres = seq_param->seq_fields.bits.enable_superres,
    .enable_cdef = seq_param->seq_fields.bits.enable_cdef,
    .enable_restoration = seq_param->seq_fields.bits.enable_restoration,
    .color_config = {
      .high_bitdepth = (seq_param->seq_fields.bits.bit_depth_minus8 > 0),
      .mono_chrome = (self->chrome == 0),
      .color_description_present_flag = 0,
      .color_primaries = GST_AV1_CP_UNSPECIFIED,
      .transfer_characteristics = GST_AV1_TC_UNSPECIFIED,
      .matrix_coefficients = GST_AV1_MC_UNSPECIFIED,
      .color_range = 0,
      .subsampling_x = seq_param->seq_fields.bits.subsampling_x,
      .subsampling_y = seq_param->seq_fields.bits.subsampling_y,
      .chroma_sample_position = 0,
      .separate_uv_delta_q = 0,
    },
    .film_grain_params_present = 0,
  };
  /* *INDENT-ON* */
}

static gboolean
_av1_add_sequence_param (GstVaAV1Enc * self, GstVaEncodePicture * picture,
    VAEncSequenceParameterBufferAV1 * sequence)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);

  if (!gst_va_encoder_add_param (base->encoder, picture,
          VAEncSequenceParameterBufferType, sequence, sizeof (*sequence))) {
    GST_ERROR_OBJECT (self, "Failed to create the sequence parameter");
    return FALSE;
  }

  return TRUE;
}

static gboolean
_av1_add_sequence_header (GstVaAV1Enc * self, GstVaAV1EncFrame * frame,
    guint * size_offset)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint size;
  guint8 packed_sps[256] = { 0, };

  size = sizeof (packed_sps);
  if (gst_av1_bit_writer_sequence_header_obu (&self->sequence_hdr, TRUE,
          packed_sps, &size)
      != GST_AV1_BIT_WRITER_OK) {
    GST_ERROR_OBJECT (self, "Failed to write sequence header.");
    return FALSE;
  }

  *size_offset += size;

  if (!gst_va_encoder_add_packed_header (base->encoder, frame->picture,
          VAEncPackedHeaderAV1_SPS, packed_sps, size * 8, FALSE)) {
    GST_ERROR_OBJECT (self, "Failed to add packed sequence header.");
    return FALSE;
  }

  return TRUE;
}

static guint8
_av1_calculate_filter_level (guint32 base_qindex, gboolean chroma)
{
  /* *INDENT-OFF* */
  static const guint8 loop_filter_levels_luma[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  2,
    2,  2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,
    4,  4,  4,  4,  5,  5,  5,  5,  5,  5,  5,  6,  6,  6,  6,  6,
    6,  7,  7,  7,  8,  8,  8,  8,  9,  9,  9,  9,  10, 10, 10, 10,
    11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15,
    15, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 19, 19, 20, 20, 20,
    21, 21, 21, 22, 22, 22, 23, 23, 24, 24, 24, 25, 25, 25, 26, 26,
    27, 27, 27, 28, 28, 29, 29, 29, 30, 30, 31, 31, 31, 32, 32, 33,
    33, 34, 34, 34, 35, 35, 36, 36, 37, 37, 38, 38, 39, 39, 40, 41,
    41, 42, 42, 43, 44, 45, 45, 46, 47, 48, 49, 50, 51, 52, 53, 55,
    56, 58, 59, 61, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
    63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
    63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63
  };

  static const guint8 loop_filter_levels_chroma[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  3,  3,
    3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
    5,  5,  5,  5,  5,  5,  5,  5,  6,  6,  6,  6,  6,  6,  6,  6,
    6,  6,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  8,  8,
    8,  8,  8,  8,  8,  8,  8,  8,  9,  9,  9,  9,  9,  9,  9,  10,
    10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15,
    16, 17, 18, 19, 20, 21, 22, 24, 25, 26, 28, 30, 31, 31, 31, 31,
    31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
    31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
    31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
    31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
    31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31
  };
  /* *INDENT-ON* */
  guint8 level = 0;

  if (!chroma) {
    level = loop_filter_levels_luma[base_qindex];
  } else {
    level = loop_filter_levels_chroma[base_qindex];
  }

  return level;
}

static void
_av1_calculate_cdef_param (GstVaAV1Enc * self,
    VAEncPictureParameterBufferAV1 * pic_param)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint32 strengths[GST_AV1_CDEF_MAX] = { 36, 50, 0, 24, 8, 17, 4, 9 };
  guint8 cdef_bits = 3;
  guint cdef_damping;
  guint i;

  /* Adjust the CDEF parameter for CQP mode. In bitrate control mode, the
     driver will update the CDEF value for each frame automatically. */
  if (self->rc.rc_ctrl_mode == VA_RC_CQP) {
    if (self->rc.base_qindex < 90) {
      /* Low QP setting. */
      strengths[0] = 5;
      strengths[1] = 41;
      strengths[3] = 6;
      strengths[5] = 16;
    } else if (self->rc.base_qindex > 140) {
      /* High QP setting. */
      cdef_bits = 2;
      strengths[1] = 63;
      if (self->rc.base_qindex > 210) {
        cdef_bits = 1;
        strengths[0] = 0;
      }
    } else {
      /* Medium QP setting. */
      cdef_bits = 2;
      strengths[1] = 63;

      if (base->width < 1600 && base->height < 1600) {
        strengths[3] = 1;
      } else {
        strengths[3] = 32;
      }
    }
  }

  cdef_damping = (self->rc.base_qindex >> 6) + 3;

  pic_param->cdef_bits = cdef_bits;
  pic_param->cdef_damping_minus_3 = cdef_damping - 3;
  for (i = 0; i < GST_AV1_CDEF_MAX; i++) {
    pic_param->cdef_y_strengths[i] = strengths[i];
    pic_param->cdef_uv_strengths[i] = strengths[i];
  }
}

static gboolean
_av1_fill_frame_param (GstVaAV1Enc * self, GstVaAV1EncFrame * va_frame,
    VAEncPictureParameterBufferAV1 * pic_param)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  gint i;
  uint8_t primary_ref_frame = GST_AV1_PRIMARY_REF_NONE;
  uint8_t refresh_frame_flags = 0xff;
  gboolean frame_is_intra;
  gboolean allow_intrabc;
  guint tx_mode;
  guint reference_mode;
  guint loop_filter_level_y;
  guint loop_filter_level_uv;

  g_assert (!(va_frame->type & FRAME_TYPE_REPEAT));

  /* *INDENT-OFF* */
  if (self->rc.rc_ctrl_mode == VA_RC_CQP) {
    loop_filter_level_y =
        _av1_calculate_filter_level (self->rc.base_qindex, FALSE);
    loop_filter_level_uv =
        _av1_calculate_filter_level (self->rc.base_qindex, TRUE);
  } else {
    /* In bitrate control mode, the driver will set the loop filter
       level for each frame, we do not care here. */
    loop_filter_level_y = 0xff;
    loop_filter_level_uv = 0xff;
  }

  tx_mode = 0;
  for (i = GST_AV1_TX_MODE_SELECT; i >= GST_AV1_TX_MODE_ONLY_4x4; i--) {
    if (self->features.tx_mode_support & (1 << i)) {
      tx_mode = i;
      break;
    }
  }
  g_assert (tx_mode != 0);

  frame_is_intra = (va_frame->type == GST_AV1_INTRA_ONLY_FRAME
      || va_frame->type == GST_AV1_KEY_FRAME);

  /* Prefer to let the driver make decision. */
  reference_mode = frame_is_intra ? 0 : (va_frame->bidir_ref ? 2 : 0);

  if (va_frame->type != GST_AV1_INTER_FRAME) {
    primary_ref_frame = GST_AV1_PRIMARY_REF_NONE;
  } else {
    /* Set it to GST_AV1_REF_LAST_FRAME */
    primary_ref_frame = GST_AV1_REF_LAST_FRAME - GST_AV1_REF_LAST_FRAME;
  }

  if (va_frame->type != GST_AV1_KEY_FRAME
      && va_frame->type != GST_AV1_SWITCH_FRAME) {
    if (va_frame->update_index >= 0) {
      refresh_frame_flags = (1 << va_frame->update_index);
    } else {
      refresh_frame_flags = 0;
    }
  }

  allow_intrabc = self->features.allow_intrabc;
  if (va_frame->type != GST_AV1_KEY_FRAME
      && va_frame->type != GST_AV1_INTRA_ONLY_FRAME)
    allow_intrabc = 0;

  *pic_param = (VAEncPictureParameterBufferAV1) {
    .frame_width_minus_1 = base->width - 1,
    .frame_height_minus_1 = base->height - 1,
    .reconstructed_frame =
        gst_va_encode_picture_get_reconstruct_surface (va_frame->picture),
    .coded_buf = va_frame->picture->coded_buffer,
    .primary_ref_frame = primary_ref_frame,
    .order_hint = va_frame->order_hint,
    .refresh_frame_flags = refresh_frame_flags,
    /* Set ref_frame_ctrl later if inter frame. */
    .ref_frame_ctrl_l0 = { .value = 0 },
    .ref_frame_ctrl_l1 = { .value = 0 },
    .picture_flags.bits = {
      .frame_type = va_frame->type,
      /* We do not support error resilient mode now. */
      .error_resilient_mode = (va_frame->type == GST_AV1_KEY_FRAME),
      .disable_cdf_update = 0,
      .use_superres = self->features.enable_superres,
      .allow_high_precision_mv = (frame_is_intra == FALSE),
      .use_ref_frame_mvs = self->features.enable_ref_frame_mvs,
      .disable_frame_end_update_cdf = 0,
      .reduced_tx_set = 0,
      /* We just use frame header + tile group mode */
      .enable_frame_obu = 0,
      .long_term_reference = 0,
      .disable_frame_recon = 0,
      .allow_intrabc = allow_intrabc,
      .palette_mode_enable = self->features.enable_palette_mode,
    },
    /* segmentation does not support now */
    .seg_id_block_size = 0,
    .num_tile_groups_minus1 = self->partition.tile_groups - 1,
    .temporal_id = va_frame->temporal_id,
    .filter_level = { loop_filter_level_y, loop_filter_level_y },
    .filter_level_u = loop_filter_level_uv,
    .filter_level_v = loop_filter_level_uv,
    .loop_filter_flags.bits = {
      .sharpness_level = 0,
      .mode_ref_delta_enabled = 0,
      .mode_ref_delta_update = 0,
    },
    .superres_scale_denominator = 0,
    .interpolation_filter = self->features.interpolation_type,
    /* Default ref deltas */
    .ref_deltas = { 1, 0, 0, 0, -1, 0, -1, -1 },
    .mode_deltas = { 0, 0 },
    .base_qindex = self->rc.base_qindex,
    /* Just set to 0. */
    .y_dc_delta_q = 0,
    .u_dc_delta_q = 0,
    .u_ac_delta_q = 0,
    .v_dc_delta_q = 0,
    .v_ac_delta_q = 0,
    .min_base_qindex = self->rc.min_qindex,
    .max_base_qindex = self->rc.max_qindex,
    .qmatrix_flags.bits = {
      .using_qmatrix = 0,
      .qm_y = 0,
      .qm_u = 0,
      .qm_v = 0,
    },
    .mode_control_flags.bits = {
      .delta_q_present = 0,
      .delta_q_res = 0,
      .delta_lf_present = 0,
      .delta_lf_res = 0,
      .delta_lf_multi = 0,
      .tx_mode = tx_mode,
      .reference_mode = reference_mode,
      .skip_mode_present = 0,
    },
    /* Do not enable segments now. */
    .segments.seg_flags.bits.segmentation_enabled =
        self->features.enable_segmentation,
    .tile_cols = self->partition.num_tile_cols,
    .tile_rows = self->partition.num_tile_rows,
    /* Set it later. */
    .width_in_sbs_minus_1 = { 0 },
    .height_in_sbs_minus_1 = { 0 },
    .context_update_tile_id = 0,
    /* CDEF parameter will be set later. */
    .cdef_damping_minus_3 = 0,
    .cdef_bits = 0,
    .cdef_y_strengths = { 0, },
    .cdef_uv_strengths = { 0, },
    .loop_restoration_flags.bits = {
      .yframe_restoration_type = 0,
      .cbframe_restoration_type = 0,
      .crframe_restoration_type = 0,
      .lr_unit_shift = 0,
      .lr_uv_shift = 0,
    },
    /* TODO: wm setting. */
    //.wm = { },
    /* Feed all the offsets later. */
    .tile_group_obu_hdr_info.bits = {
      .obu_extension_flag = 0,
      .obu_has_size_field = 1,
      .temporal_id = va_frame->temporal_id,
      .spatial_id = va_frame->spatial_id,
    },
    .number_skip_frames = 0,
    .skip_frames_reduced_size = 0,
  };
  /* *INDENT-ON* */

  _av1_calculate_cdef_param (self, pic_param);

  for (i = 0; i < self->partition.num_tile_cols - 1; i++)
    pic_param->width_in_sbs_minus_1[i] = self->partition.tile_width_sb - 1;
  pic_param->width_in_sbs_minus_1[i] = self->partition.sb_cols -
      (self->partition.num_tile_cols - 1) * self->partition.tile_width_sb - 1;

  for (i = 0; i < self->partition.num_tile_rows - 1; i++)
    pic_param->height_in_sbs_minus_1[i] = self->partition.tile_height_sb - 1;
  pic_param->height_in_sbs_minus_1[i] = self->partition.sb_rows -
      (self->partition.num_tile_rows - 1) * self->partition.tile_height_sb - 1;

  if (va_frame->type == GST_AV1_INTER_FRAME) {
    for (i = 0; i < 8; i++) {
      if (self->gop.ref_list[i] == NULL) {
        pic_param->reference_frames[i] = VA_INVALID_SURFACE;
        continue;
      }

      pic_param->reference_frames[i] =
          gst_va_encode_picture_get_reconstruct_surface
          (_enc_frame (self->gop.ref_list[i])->picture);
    }

    for (i = 0; i < 7; i++) {
      if (va_frame->ref_frame_idx[i + GST_AV1_REF_LAST_FRAME] == -1) {
        pic_param->ref_frame_idx[i] = 0xFF;
        continue;
      }

      pic_param->ref_frame_idx[i] =
          va_frame->ref_frame_idx[i + GST_AV1_REF_LAST_FRAME];
    }

    g_assert (va_frame->ref_frame_idx[GST_AV1_REF_GOLDEN_FRAME] != -1);
    pic_param->ref_frame_ctrl_l0.fields.search_idx0 = GST_AV1_REF_LAST_FRAME;

    if (va_frame->bidir_ref) {
      if (va_frame->ref_frame_idx[GST_AV1_REF_ALTREF_FRAME] != -1)
        pic_param->ref_frame_ctrl_l1.fields.search_idx0 =
            GST_AV1_REF_BWDREF_FRAME;
    }
  } else {
    for (i = 0; i < 8; i++)
      pic_param->reference_frames[i] = VA_INVALID_SURFACE;
    for (i = 0; i < 7; i++)
      pic_param->ref_frame_idx[i] = 0xFF;
  }

  return TRUE;
}

/* 5.9.3 */
static gint
_av1_get_relative_dist (GstAV1SequenceHeaderOBU * seq_header, gint a, gint b)
{
  gint m, diff;
  if (!seq_header->enable_order_hint)
    return 0;

  diff = a - b;
  m = 1 << seq_header->order_hint_bits_minus_1;
  diff = (diff & (m - 1)) - (diff & m);
  return diff;
}

/* We need to calculate whether the skip mode is available */
static void
_av1_set_skip_mode_frame (GstVaAV1Enc * self, GstVaAV1EncFrame * va_frame,
    GstAV1FrameHeaderOBU * frame_hdr)
{
  GstAV1SequenceHeaderOBU *seq_header;
  gint skip_mode_allowed /* skipModeAllowed */ ;
  GstVideoCodecFrame *ref_frame;
  guint i;

  seq_header = &self->sequence_hdr;
  skip_mode_allowed = 0;

  if (frame_hdr->frame_is_intra || !frame_hdr->reference_select
      || !seq_header->enable_order_hint) {
    skip_mode_allowed = 0;
  } else {
    gint forward_idx = -1 /* forwardIdx */ ;
    gint forward_hint = 0 /* forwardHint */ ;
    gint backward_idx = -1 /* backwardIdx */ ;
    gint backward_hint = 0 /* backwardHint */ ;
    gint ref_hint = 0 /* refHint */ ;

    for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
      ref_frame = self->gop.ref_list[va_frame->ref_frame_idx[i +
              GST_AV1_REF_LAST_FRAME]];
      g_assert (ref_frame);
      ref_hint = _enc_frame (ref_frame)->order_hint;

      if (_av1_get_relative_dist (seq_header, ref_hint,
              frame_hdr->order_hint) < 0) {
        if (forward_idx < 0 || _av1_get_relative_dist (seq_header, ref_hint,
                forward_hint) > 0) {
          forward_idx = i;
          forward_hint = ref_hint;
        }
      } else
          if (_av1_get_relative_dist (seq_header, ref_hint,
              frame_hdr->order_hint) > 0) {
        if (backward_idx < 0 || _av1_get_relative_dist (seq_header, ref_hint,
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
      frame_hdr->skip_mode_frame[0] =
          GST_AV1_REF_LAST_FRAME + MIN (forward_idx, backward_idx);
      frame_hdr->skip_mode_frame[1] =
          GST_AV1_REF_LAST_FRAME + MAX (forward_idx, backward_idx);
    } else {
      gint second_forward_idx = -1 /* secondForwardIdx */ ;
      gint second_forward_hint = 0 /* secondForwardHint */ ;

      for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
        ref_frame = self->gop.ref_list[va_frame->ref_frame_idx[i +
                GST_AV1_REF_LAST_FRAME]];
        g_assert (ref_frame);
        ref_hint = _enc_frame (ref_frame)->order_hint;

        if (_av1_get_relative_dist (seq_header, ref_hint, forward_hint) < 0) {
          if (second_forward_idx < 0 || _av1_get_relative_dist (seq_header,
                  ref_hint, second_forward_hint) > 0) {
            second_forward_idx = i;
            second_forward_hint = ref_hint;
          }
        }
      }

      if (second_forward_idx < 0) {
        skip_mode_allowed = 0;
      } else {
        skip_mode_allowed = 1;
        frame_hdr->skip_mode_frame[0] =
            GST_AV1_REF_LAST_FRAME + MIN (forward_idx, second_forward_idx);
        frame_hdr->skip_mode_frame[1] =
            GST_AV1_REF_LAST_FRAME + MAX (forward_idx, second_forward_idx);
      }
    }
  }

  if (skip_mode_allowed) {
    g_assert (frame_hdr->skip_mode_frame[0] > 0 &&
        frame_hdr->skip_mode_frame[1] > 0);
  } else {
    frame_hdr->skip_mode_frame[0] = 0;
    frame_hdr->skip_mode_frame[1] = 0;
  }
}

static void
_av1_fill_frame_header (GstVaAV1Enc * self,
    VAEncPictureParameterBufferAV1 * pic_param,
    GstAV1FrameHeaderOBU * frame_hdr, GstVaAV1EncFrame * va_frame)
{
  guint i;
  guint8 frame_is_intra = (va_frame->type == GST_AV1_INTRA_ONLY_FRAME
      || va_frame->type == GST_AV1_KEY_FRAME);

  /* *INDENT-OFF* */
  *frame_hdr = (GstAV1FrameHeaderOBU) {
    .frame_is_intra = frame_is_intra,
    .show_existing_frame = 0,
    .frame_type = va_frame->type,
    .show_frame = !(va_frame->flags & FRAME_FLAG_NOT_SHOW),
    .showable_frame = 1,
    .error_resilient_mode = pic_param->picture_flags.bits.error_resilient_mode,
    .disable_cdf_update = pic_param->picture_flags.bits.disable_cdf_update,
    .allow_screen_content_tools = 0,
    .frame_size_override_flag = 0,
    .frame_width = self->sequence_hdr.max_frame_width_minus_1 + 1,
    .frame_height = self->sequence_hdr.max_frame_height_minus_1 + 1,
    .order_hint = pic_param->order_hint,
    .primary_ref_frame = pic_param->primary_ref_frame,
    .buffer_removal_time_present_flag = 0,
    .refresh_frame_flags = pic_param->refresh_frame_flags,
    .allow_intrabc = pic_param->picture_flags.bits.allow_intrabc,
    .frame_refs_short_signaling = 0,
    /* Set ref_frame_idx later. */
    .ref_frame_idx = { 0, },
    .allow_high_precision_mv =
        pic_param->picture_flags.bits.allow_high_precision_mv,
    .is_motion_mode_switchable = 0,
    .use_ref_frame_mvs = pic_param->picture_flags.bits.use_ref_frame_mvs,
    .disable_frame_end_update_cdf =
        pic_param->picture_flags.bits.disable_frame_end_update_cdf,
    .allow_warped_motion = self->features.enable_warped_motion,
    .reduced_tx_set = pic_param->picture_flags.bits.reduced_tx_set,
    .render_and_frame_size_different = 0,
    .use_superres = pic_param->picture_flags.bits.use_superres,
    .is_filter_switchable = (pic_param->interpolation_filter ==
        GST_AV1_INTERPOLATION_FILTER_SWITCHABLE),
    .interpolation_filter = pic_param->interpolation_filter,
    .loop_filter_params = {
      .loop_filter_level[0] = pic_param->filter_level[0],
      .loop_filter_level[1] = pic_param->filter_level[1],
      .loop_filter_level[2] = pic_param->filter_level_u,
      .loop_filter_level[3] = pic_param->filter_level_v,
      .loop_filter_sharpness =
          pic_param->loop_filter_flags.bits.sharpness_level,
      .loop_filter_delta_enabled =
          pic_param->loop_filter_flags.bits.mode_ref_delta_enabled,
      .loop_filter_delta_update =
          pic_param->loop_filter_flags.bits.mode_ref_delta_update,
      /* Set it later. */
      .loop_filter_ref_deltas = { 0, },
      .loop_filter_mode_deltas = { 0, },
      .delta_lf_present = pic_param->mode_control_flags.bits.delta_lf_present,
      .delta_lf_res = pic_param->mode_control_flags.bits.delta_lf_res,
      .delta_lf_multi = pic_param->mode_control_flags.bits.delta_lf_multi,
    },
    .quantization_params = {
      .base_q_idx = pic_param->base_qindex,
      .diff_uv_delta = 0,
      .using_qmatrix = pic_param->qmatrix_flags.bits.using_qmatrix,
      .qm_y = pic_param->qmatrix_flags.bits.qm_y,
      .qm_u = pic_param->qmatrix_flags.bits.qm_u,
      .qm_v = pic_param->qmatrix_flags.bits.qm_v,
      .delta_q_present = pic_param->mode_control_flags.bits.delta_q_present,
      .delta_q_res = pic_param->mode_control_flags.bits.delta_q_res,
      .delta_q_y_dc = pic_param->y_dc_delta_q,
      .delta_q_u_dc = pic_param->u_dc_delta_q,
      .delta_q_u_ac = pic_param->u_ac_delta_q,
      .delta_q_v_dc = pic_param->v_dc_delta_q,
      .delta_q_v_ac = pic_param->v_ac_delta_q,
    },
    .segmentation_params = {
      /* Not enabled. */
      .segmentation_enabled =
          pic_param->segments.seg_flags.bits.segmentation_enabled,
    },
    .tile_info = {
      .uniform_tile_spacing_flag = 1,
      .tile_cols_log2 = self->partition.tile_cols_log2,
      .tile_rows_log2 = self->partition.tile_rows_log2,
      .context_update_tile_id = 0,
      .tile_size_bytes_minus_1 = self->partition.tile_size_bytes_minus_1,
    },
    .cdef_params = {
      .cdef_damping = pic_param->cdef_damping_minus_3 + 3,
      .cdef_bits = pic_param->cdef_bits,
      /* Set to later. */
      .cdef_y_pri_strength = { 0, },
      .cdef_y_sec_strength = { 0, },
      .cdef_uv_pri_strength = { 0, },
      .cdef_uv_sec_strength = { 0, },
    },
    /* TODO: disable seq->enable_restoration now. */
    .loop_restoration_params = { 0, },
    .tx_mode = pic_param->mode_control_flags.bits.tx_mode,
    .skip_mode_present = pic_param->mode_control_flags.bits.skip_mode_present,
    .reference_select = pic_param->mode_control_flags.bits.reference_mode != 0,
    /* warped motion is not supported. */
    .global_motion_params = {
      .gm_type = {
        GST_AV1_WARP_MODEL_IDENTITY,
        GST_AV1_WARP_MODEL_IDENTITY,
        GST_AV1_WARP_MODEL_IDENTITY,
        GST_AV1_WARP_MODEL_IDENTITY,
        GST_AV1_WARP_MODEL_IDENTITY,
        GST_AV1_WARP_MODEL_IDENTITY,
        GST_AV1_WARP_MODEL_IDENTITY,
        GST_AV1_WARP_MODEL_IDENTITY
      },
    },
    /* film grain is not supported. */
    .film_grain_params = {
      .apply_grain = FALSE,
    },
  };
  /* *INDENT-ON* */

  for (i = 0; i < GST_AV1_CDEF_MAX; i++) {
    frame_hdr->cdef_params.cdef_y_pri_strength[i] =
        pic_param->cdef_y_strengths[i] / 4;
    frame_hdr->cdef_params.cdef_y_sec_strength[i] =
        pic_param->cdef_y_strengths[i] % 4;
    frame_hdr->cdef_params.cdef_uv_pri_strength[i] =
        pic_param->cdef_uv_strengths[i] / 4;
    frame_hdr->cdef_params.cdef_uv_sec_strength[i] =
        pic_param->cdef_uv_strengths[i] % 4;
  }

  _av1_set_skip_mode_frame (self, va_frame, frame_hdr);

  for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++)
    frame_hdr->ref_frame_idx[i] = pic_param->ref_frame_idx[i];

  for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++)
    frame_hdr->loop_filter_params.loop_filter_ref_deltas[i] =
        pic_param->ref_deltas[i];
  for (i = 0; i < 2; i++)
    frame_hdr->loop_filter_params.loop_filter_mode_deltas[i] =
        pic_param->mode_deltas[i];
}

static gboolean
_av1_add_tile_group_param (GstVaAV1Enc * self, GstVaAV1EncFrame * va_frame,
    guint index)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  VAEncTileGroupBufferAV1 tile_group_param;
  guint div;

  div = self->partition.num_tile_cols * self->partition.num_tile_rows /
      self->partition.tile_groups;

  tile_group_param.tg_start = div * index;

  if (index == self->partition.tile_groups - 1) {
    tile_group_param.tg_end =
        self->partition.num_tile_cols * self->partition.num_tile_rows - 1;
  } else {
    tile_group_param.tg_end = (index + 1) * div - 1;
  }

  if (!gst_va_encoder_add_param (base->encoder, va_frame->picture,
          VAEncSliceParameterBufferType, &tile_group_param,
          sizeof (VAEncTileGroupBufferAV1))) {
    GST_ERROR_OBJECT (self, "Failed to add one tile group parameter");
    return FALSE;
  }

  return TRUE;
}

static gboolean
_av1_encode_one_frame (GstVaAV1Enc * self, GstVaAV1EncFrame * va_frame,
    guint size_offset)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  VAEncPictureParameterBufferAV1 pic_param;
  GstAV1FrameHeaderOBU frame_hdr;
  guint frame_hdr_size;
  guint8 packed_frame_hdr[512] = { 0, };
  guint i;

  va_frame->order_hint = va_frame->frame_num;

  if (!_av1_fill_frame_param (self, va_frame, &pic_param)) {
    GST_ERROR_OBJECT (self, "Fails to fill the frame parameter.");
    return FALSE;
  }

  _av1_fill_frame_header (self, &pic_param, &frame_hdr, va_frame);

  frame_hdr_size = sizeof (packed_frame_hdr);

  if (self->packed_headers & VA_ENC_PACKED_HEADER_PICTURE) {
    if (self->rc.rc_ctrl_mode == VA_RC_CQP) {
      if (gst_av1_bit_writer_frame_header_obu (&frame_hdr,
              &self->sequence_hdr, va_frame->temporal_id, va_frame->spatial_id,
              TRUE, packed_frame_hdr, &frame_hdr_size)
          != GST_AV1_BIT_WRITER_OK) {
        GST_ERROR_OBJECT (self, "Failed to write frame header.");
        return FALSE;
      }
    } else {
      guint qindex_offset = 0;
      guint segmentation_offset = 0;
      guint loopfilter_offset = 0;
      guint cdef_offset = 0;
      guint cdef_size_in_bits = 0;

      /* For rate control modes, the driver needs to adjust the values of
         qindex, loop filter, etc. The accroding fields of frame header are
         modified by the driver. And so the total frame header size may
         also change and need rewrite. */
      if (gst_av1_bit_writer_frame_header_obu_with_offsets (&frame_hdr,
              &self->sequence_hdr, va_frame->temporal_id, va_frame->spatial_id,
              TRUE, self->features.obu_size_bytes, &qindex_offset,
              &segmentation_offset, &loopfilter_offset, &cdef_offset,
              &cdef_size_in_bits, packed_frame_hdr, &frame_hdr_size)
          != GST_AV1_BIT_WRITER_OK) {
        GST_ERROR_OBJECT (self, "Failed to write frame header.");
        return FALSE;
      }

      /* Fix all the offsets based on the packed frame header */
      pic_param.bit_offset_qindex = qindex_offset;
      pic_param.bit_offset_segmentation = segmentation_offset;
      pic_param.bit_offset_loopfilter_params = loopfilter_offset;
      pic_param.bit_offset_cdef_params = cdef_offset;
      pic_param.size_in_bits_cdef_params = cdef_size_in_bits;
      pic_param.byte_offset_frame_hdr_obu_size = size_offset + 1 +
          /* OBU extension header */
          (va_frame->temporal_id > 0 || va_frame->spatial_id > 0);
      pic_param.size_in_bits_frame_hdr_obu = frame_hdr_size * 8;
    }
  }

  if (!gst_va_encoder_add_param (base->encoder, va_frame->picture,
          VAEncPictureParameterBufferType, &pic_param, sizeof (pic_param))) {
    GST_ERROR_OBJECT (self, "Failed to create the frame parameter");
    return FALSE;
  }

  if ((self->packed_headers & VA_ENC_PACKED_HEADER_PICTURE) &&
      !gst_va_encoder_add_packed_header (base->encoder, va_frame->picture,
          VAEncPackedHeaderAV1_PPS, packed_frame_hdr, frame_hdr_size * 8,
          FALSE)) {
    GST_ERROR_OBJECT (self, "Failed to add the packed frame header");
    return FALSE;
  }

  for (i = 0; i < self->partition.tile_groups; i++) {
    if (!_av1_add_tile_group_param (self, va_frame, i)) {
      GST_ERROR_OBJECT (self, "Failed to add tile groups");
      return FALSE;
    }
  }

  if (!gst_va_encoder_encode (base->encoder, va_frame->picture)) {
    GST_ERROR_OBJECT (self, "Encode frame error");
    return FALSE;
  }

  return TRUE;
}

static void
_av1_add_td (GstVaAV1Enc * self, GstVaAV1EncFrame * va_frame)
{
  guint td_data_size;

  td_data_size = sizeof (va_frame->cached_frame_header) -
      va_frame->cached_frame_header_size;

  if (gst_av1_bit_writer_temporal_delimiter_obu (TRUE,
          va_frame->cached_frame_header + va_frame->cached_frame_header_size,
          &td_data_size) != GST_AV1_BIT_WRITER_OK) {
    GST_ERROR_OBJECT (self, "Failed to write temporal delimiter.");
    /* The only possible failure is not enough buffer size,
       user should ensure that. */
    g_assert_not_reached ();
  }

  va_frame->cached_frame_header_size += td_data_size;
}

static void
_av1_add_repeat_frame_header (GstVaAV1Enc * self, GstVaAV1EncFrame * va_frame)
{
  GstAV1FrameHeaderOBU frame_hdr = { 0, };
  guint frame_hdr_data_size;

  /* Repeat frame always shows a frame and so begins with a TD. */
  _av1_add_td (self, va_frame);

  frame_hdr.show_existing_frame = 1;
  frame_hdr.frame_to_show_map_idx = va_frame->repeat_index;

  frame_hdr_data_size = sizeof (va_frame->cached_frame_header) -
      va_frame->cached_frame_header_size;

  if (gst_av1_bit_writer_frame_header_obu (&frame_hdr, &self->sequence_hdr,
          va_frame->temporal_id, va_frame->spatial_id, TRUE,
          va_frame->cached_frame_header + va_frame->cached_frame_header_size,
          &frame_hdr_data_size) != GST_AV1_BIT_WRITER_OK) {
    GST_ERROR_OBJECT (self, "Failed to write repeat frame header.");
    g_assert_not_reached ();
  }

  va_frame->cached_frame_header_size += frame_hdr_data_size;
}

static GstFlowReturn
gst_va_av1_enc_encode_frame (GstVaBaseEnc * base,
    GstVideoCodecFrame * gst_frame, gboolean is_last)
{
  GstVaAV1Enc *self = GST_VA_AV1_ENC (base);
  GstVaAV1EncFrame *va_frame = _enc_frame (gst_frame);
  VAEncSequenceParameterBufferAV1 seq_param;

  if (!_av1_assign_ref_index (self, gst_frame)) {
    GST_ERROR_OBJECT (self, "Failed to assign reference for frame:"
        "system_frame_number %d, frame_num: %d, frame_type %s",
        gst_frame->system_frame_number, va_frame->frame_num,
        _av1_get_frame_type_name (va_frame->type));
    return GST_FLOW_ERROR;
  }

  memset (va_frame->cached_frame_header, 0,
      sizeof (va_frame->cached_frame_header));
  va_frame->cached_frame_header_size = 0;

  if (va_frame->type & FRAME_TYPE_REPEAT) {
    g_assert (va_frame->flags & FRAME_FLAG_ALREADY_ENCODED);
    _av1_add_repeat_frame_header (self, va_frame);
    self->inside_tu = FALSE;
  } else {
    guint size_offset = 0;

    g_assert (va_frame->picture == NULL);
    va_frame->picture = gst_va_encode_picture_new (base->encoder,
        gst_frame->input_buffer);

    _av1_find_ref_to_update (base, gst_frame);

    if (!self->inside_tu &&
        (self->packed_headers & VA_ENC_PACKED_HEADER_RAW_DATA)) {
      _av1_add_td (self, va_frame);
    }

    if (va_frame->flags & FRAME_FLAG_NOT_SHOW) {
      if (!self->inside_tu)
        self->inside_tu = TRUE;
    } else {
      self->inside_tu = FALSE;
    }

    /* Repeat the sequence for each key. */
    if (va_frame->frame_num == 0) {
      if (!gst_va_base_enc_add_rate_control_parameter (base, va_frame->picture,
              self->rc.rc_ctrl_mode, self->rc.max_bitrate_bits,
              self->rc.target_percentage, self->rc.base_qindex,
              self->rc.min_qindex, self->rc.max_qindex, self->rc.mbbrc))
        return FALSE;

      if (!gst_va_base_enc_add_quality_level_parameter (base, va_frame->picture,
              self->rc.target_usage))
        return FALSE;

      if (!gst_va_base_enc_add_frame_rate_parameter (base, va_frame->picture))
        return FALSE;

      if (!gst_va_base_enc_add_hrd_parameter (base, va_frame->picture,
              self->rc.rc_ctrl_mode, self->rc.cpb_length_bits))
        return FALSE;

      _av1_fill_sequence_param (self, &seq_param);
      if (!_av1_add_sequence_param (self, va_frame->picture, &seq_param))
        return FALSE;

      _av1_fill_sequence_header (self, &seq_param);
      if ((self->packed_headers & VA_ENC_PACKED_HEADER_SEQUENCE) &&
          !_av1_add_sequence_header (self, va_frame, &size_offset))
        return FALSE;
    }

    if (!_av1_encode_one_frame (self, va_frame, size_offset)) {
      GST_ERROR_OBJECT (self, "Fails to encode one frame.");
      return GST_FLOW_ERROR;
    }

    va_frame->flags |= FRAME_FLAG_ALREADY_ENCODED;
  }

  _av1_update_ref_list (base, gst_frame);

  g_queue_push_tail (&base->output_list, gst_video_codec_frame_ref (gst_frame));

  return GST_FLOW_OK;
}

static GstBuffer *
_av1_create_tu_output_buffer (GstVaAV1Enc * self,
    GstVideoCodecFrame * last_frame)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint8 *data;
  guint total_sz, offset;
  gint frame_size;
  GstVaAV1EncFrame *frame_enc;
  GstBuffer *buf = NULL;
  guint num;

  g_assert ((_enc_frame (last_frame)->flags & FRAME_TYPE_REPEAT) == 0);
  g_assert ((_enc_frame (last_frame)->flags & FRAME_FLAG_NOT_SHOW) == 0);
  g_assert (self->frames_in_tu_num <= GST_AV1_NUM_REF_FRAMES - 1);

  /* cached_frame_header + max codedbuf_size for each */
  total_sz = (self->frames_in_tu_num + 1) * (32 + base->codedbuf_size);

  data = g_malloc (total_sz);
  if (!data)
    goto error;

  offset = 0;
  for (num = 0; num < self->frames_in_tu_num; num++) {
    frame_enc = _enc_frame (self->frames_in_tu[num]);

    if (frame_enc->cached_frame_header_size > 0) {
      memcpy (data + offset, frame_enc->cached_frame_header,
          frame_enc->cached_frame_header_size);
      offset += frame_enc->cached_frame_header_size;
    }

    frame_size = gst_va_base_enc_copy_output_data (base,
        frame_enc->picture, data + offset, total_sz - offset);
    if (frame_size <= 0) {
      GST_ERROR_OBJECT (self, "Fails to copy the output data of "
          "system_frame_number %d, frame_num: %d",
          self->frames_in_tu[num]->system_frame_number, frame_enc->frame_num);
      goto error;
    }

    offset += frame_size;
  }

  frame_enc = _enc_frame (last_frame);

  if (frame_enc->cached_frame_header_size > 0) {
    memcpy (data + offset, frame_enc->cached_frame_header,
        frame_enc->cached_frame_header_size);
    offset += frame_enc->cached_frame_header_size;
  }

  frame_size = gst_va_base_enc_copy_output_data (base,
      frame_enc->picture, data + offset, total_sz - offset);
  if (frame_size <= 0) {
    GST_ERROR_OBJECT (self, "Fails to copy the output data of "
        "system_frame_number %d, frame_num: %d",
        last_frame->system_frame_number, frame_enc->frame_num);
    goto error;
  }
  offset += frame_size;
  num++;

  buf = gst_video_encoder_allocate_output_buffer
      (GST_VIDEO_ENCODER_CAST (base), offset);
  if (!buf) {
    GST_ERROR_OBJECT (base, "Failed to create output buffer");
    goto error;
  }

  if (gst_buffer_fill (buf, 0, data, offset) != offset) {
    GST_ERROR_OBJECT (base, "Failed to write output buffer for TU");
    goto error;
  }

  g_free (data);

  _av1_clear_frames_in_tu (self);

  return buf;

error:
  {
    if (data)
      g_free (data);

    _av1_clear_frames_in_tu (self);
    gst_clear_buffer (&buf);

    return NULL;
  }
}

static gboolean
gst_va_av1_enc_prepare_output (GstVaBaseEnc * base,
    GstVideoCodecFrame * frame, gboolean * complete)
{
  GstVaAV1Enc *self = GST_VA_AV1_ENC (base);
  GstVaAV1EncFrame *frame_enc;
  GstBuffer *buf = NULL;

  frame_enc = _enc_frame (frame);

  if (frame_enc->flags & FRAME_FLAG_NOT_SHOW &&
      (frame_enc->flags & FRAME_FLAG_ALREADY_OUTPUTTED) == 0) {
    g_assert (self->frames_in_tu_num <= GST_AV1_NUM_REF_FRAMES - 1);
    self->frames_in_tu[self->frames_in_tu_num] = frame;
    self->frames_in_tu_num++;

    g_assert ((frame_enc->flags & FRAME_FLAG_FRAME_IN_TU_CACHE) == 0);
    frame_enc->flags |= FRAME_FLAG_FRAME_IN_TU_CACHE;
    frame_enc->flags |= FRAME_FLAG_ALREADY_OUTPUTTED;

    *complete = FALSE;

    gst_buffer_replace (&frame->output_buffer, NULL);

    return TRUE;
  }

  if (frame_enc->flags & FRAME_FLAG_ALREADY_OUTPUTTED) {
    gsize sz;

    /* Already outputted, must be a repeat this time. */
    g_assert (frame_enc->type & FRAME_TYPE_REPEAT);
    /* Should already sync and complete in previous TU. */
    g_assert ((frame_enc->flags & FRAME_FLAG_FRAME_IN_TU_CACHE) == 0);

    buf = gst_video_encoder_allocate_output_buffer
        (GST_VIDEO_ENCODER_CAST (base), frame_enc->cached_frame_header_size);
    if (!buf) {
      GST_ERROR_OBJECT (base, "Failed to create output buffer");
      return FALSE;
    }

    sz = gst_buffer_fill (buf, 0, frame_enc->cached_frame_header,
        frame_enc->cached_frame_header_size);

    if (sz != frame_enc->cached_frame_header_size) {
      GST_ERROR_OBJECT (base, "Failed to write output buffer for repeat frame");
      gst_clear_buffer (&buf);
      return FALSE;
    }
  } else {
    if (self->frames_in_tu_num > 0) {
      buf = _av1_create_tu_output_buffer (self, frame);
    } else {
      buf = gst_va_base_enc_create_output_buffer (base, frame_enc->picture,
          (frame_enc->cached_frame_header_size > 0 ?
              frame_enc->cached_frame_header : NULL),
          frame_enc->cached_frame_header_size);
    }
    if (!buf) {
      GST_ERROR_OBJECT (base, "Failed to create output buffer%s",
          self->frames_in_tu_num > 0 ? " for TU" : "");
      return FALSE;
    }

    frame_enc->flags |= FRAME_FLAG_ALREADY_OUTPUTTED;
  }

  *complete = TRUE;

  GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_MARKER);
  if (frame_enc->frame_num == 0) {
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_HEADER);
    GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);
    GST_BUFFER_FLAG_UNSET (buf, GST_BUFFER_FLAG_DELTA_UNIT);
  } else {
    GST_VIDEO_CODEC_FRAME_UNSET_SYNC_POINT (frame);
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT);
  }

  gst_buffer_replace (&frame->output_buffer, buf);
  gst_clear_buffer (&buf);

  return TRUE;
}

/* *INDENT-OFF* */
static const gchar *sink_caps_str =
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_VA,
        "{ NV12 }") " ;"
    GST_VIDEO_CAPS_MAKE ("{ NV12 }");
/* *INDENT-ON* */

static const gchar *src_caps_str = "video/x-av1,alignment=(string)tu,"
    "stream-format=(string)obu-stream";

static gpointer
_register_debug_category (gpointer data)
{
  GST_DEBUG_CATEGORY_INIT (gst_va_av1enc_debug, "vaav1enc", 0,
      "VA av1 encoder");

  return NULL;
}

static void
gst_va_av1_enc_init (GTypeInstance * instance, gpointer g_class)
{
  GstVaAV1Enc *self = GST_VA_AV1_ENC (instance);

  /* default values */
  self->prop.bitrate = 0;
  self->prop.target_usage = 4;
  self->prop.cpb_size = 0;
  self->prop.target_percentage = 66;
  self->prop.gf_group_size = MAX_GF_GROUP_SIZE;
  self->prop.num_ref_frames = 7;
  self->prop.max_hierarchical_level = HIGHEST_PYRAMID_LEVELS;
  self->prop.use_128x128_superblock = FALSE;
  self->prop.keyframe_interval = MAX_KEY_FRAME_INTERVAL;
  self->prop.qp = DEFAULT_BASE_QINDEX;
  self->prop.min_qp = 0;
  self->prop.max_qp = 255;
  self->prop.num_tile_cols = 1;
  self->prop.num_tile_rows = 1;
  self->prop.tile_groups = 1;
  self->prop.mbbrc = 0;

  if (properties[PROP_RATE_CONTROL]) {
    self->prop.rc_ctrl =
        G_PARAM_SPEC_ENUM (properties[PROP_RATE_CONTROL])->default_value;
  } else {
    self->prop.rc_ctrl = VA_RC_NONE;
  }
}

static void
gst_va_av1_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVaAV1Enc *const self = GST_VA_AV1_ENC (object);
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);

  if (base->encoder && gst_va_encoder_is_open (base->encoder)) {
    GST_ERROR_OBJECT (object,
        "failed to set any property after encoding started");
    return;
  }

  GST_OBJECT_LOCK (self);

  switch (prop_id) {
    case PROP_KEYFRAME_INT:
      self->prop.keyframe_interval = g_value_get_uint (value);
      break;
    case PROP_GOLDEN_GROUP_SIZE:
      self->prop.gf_group_size = g_value_get_uint (value);
      break;
    case PROP_NUM_REF_FRAMES:
      self->prop.num_ref_frames = g_value_get_uint (value);
      break;
    case PROP_HIERARCHICAL_LEVEL:
      self->prop.max_hierarchical_level = g_value_get_uint (value);
      break;
    case PROP_128X128_SUPERBLOCK:
      self->prop.use_128x128_superblock = g_value_get_boolean (value);
      break;
    case PROP_QP:
      self->prop.qp = g_value_get_uint (value);
      break;
    case PROP_MAX_QP:
      self->prop.max_qp = g_value_get_uint (value);
      break;
    case PROP_MIN_QP:
      self->prop.min_qp = g_value_get_uint (value);
      break;
    case PROP_BITRATE:
      self->prop.bitrate = g_value_get_uint (value);
      break;
    case PROP_NUM_TILE_COLS:
      self->prop.num_tile_cols = g_value_get_uint (value);
      break;
    case PROP_NUM_TILE_ROWS:
      self->prop.num_tile_rows = g_value_get_uint (value);
      break;
    case PROP_TILE_GROUPS:
      self->prop.tile_groups = g_value_get_uint (value);
      break;
    case PROP_TARGET_USAGE:
      self->prop.target_usage = g_value_get_uint (value);
      break;
    case PROP_TARGET_PERCENTAGE:
      self->prop.target_percentage = g_value_get_uint (value);
      break;
    case PROP_CPB_SIZE:
      self->prop.cpb_size = g_value_get_uint (value);
      break;
    case PROP_RATE_CONTROL:
      self->prop.rc_ctrl = g_value_get_enum (value);
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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }

  GST_OBJECT_UNLOCK (self);
}

static void
gst_va_av1_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVaAV1Enc *const self = GST_VA_AV1_ENC (object);

  GST_OBJECT_LOCK (self);

  switch (prop_id) {
    case PROP_KEYFRAME_INT:
      g_value_set_uint (value, self->prop.keyframe_interval);
      break;
    case PROP_GOLDEN_GROUP_SIZE:
      g_value_set_uint (value, self->prop.gf_group_size);
      break;
    case PROP_NUM_REF_FRAMES:
      g_value_set_uint (value, self->prop.num_ref_frames);
      break;
    case PROP_HIERARCHICAL_LEVEL:
      g_value_set_uint (value, self->prop.max_hierarchical_level);
      break;
    case PROP_128X128_SUPERBLOCK:
      g_value_set_boolean (value, self->prop.use_128x128_superblock);
      break;
    case PROP_QP:
      g_value_set_uint (value, self->prop.qp);
      break;
    case PROP_MIN_QP:
      g_value_set_uint (value, self->prop.min_qp);
      break;
    case PROP_MAX_QP:
      g_value_set_uint (value, self->prop.max_qp);
      break;
    case PROP_NUM_TILE_COLS:
      g_value_set_uint (value, self->prop.num_tile_cols);
      break;
    case PROP_NUM_TILE_ROWS:
      g_value_set_uint (value, self->prop.num_tile_rows);
      break;
    case PROP_TILE_GROUPS:
      g_value_set_uint (value, self->prop.tile_groups);
      break;
    case PROP_BITRATE:
      g_value_set_uint (value, self->prop.bitrate);
      break;
    case PROP_TARGET_USAGE:
      g_value_set_uint (value, self->prop.target_usage);
      break;
    case PROP_TARGET_PERCENTAGE:
      g_value_set_uint (value, self->prop.target_percentage);
      break;
    case PROP_CPB_SIZE:
      g_value_set_uint (value, self->prop.cpb_size);
      break;
    case PROP_RATE_CONTROL:
      g_value_set_enum (value, self->prop.rc_ctrl);
      break;
    case PROP_MBBRC:
      g_value_set_enum (value, self->prop.mbbrc);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }

  GST_OBJECT_UNLOCK (self);
}

static void
gst_va_av1_enc_class_init (gpointer g_klass, gpointer class_data)
{
  GstCaps *src_doc_caps, *sink_doc_caps;
  GstPadTemplate *sink_pad_templ, *src_pad_templ;
  GObjectClass *object_class = G_OBJECT_CLASS (g_klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_klass);
  GstVideoEncoderClass *venc_class = GST_VIDEO_ENCODER_CLASS (g_klass);
  GstVaBaseEncClass *va_enc_class = GST_VA_BASE_ENC_CLASS (g_klass);
  GstVaAV1EncClass *vaav1enc_class = GST_VA_AV1_ENC_CLASS (g_klass);
  GstVaDisplay *display;
  GstVaEncoder *encoder;
  struct CData *cdata = class_data;
  gchar *long_name;
  const gchar *name, *desc;
  gint n_props = N_PROPERTIES;

  if (cdata->entrypoint == VAEntrypointEncSlice) {
    desc = "VA-API based AV1 video encoder";
    name = "VA-API AV1 Encoder";
  } else {
    desc = "VA-API based AV1 low power video encoder";
    name = "VA-API AV1 Low Power Encoder";
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

  va_enc_class->codec = AV1;
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

  object_class->set_property = gst_va_av1_enc_set_property;
  object_class->get_property = gst_va_av1_enc_get_property;

  venc_class->flush = GST_DEBUG_FUNCPTR (gst_va_av1_enc_flush);
  va_enc_class->reset_state = GST_DEBUG_FUNCPTR (gst_va_av1_enc_reset_state);
  va_enc_class->reconfig = GST_DEBUG_FUNCPTR (gst_va_av1_enc_reconfig);
  va_enc_class->new_frame = GST_DEBUG_FUNCPTR (gst_va_av1_enc_new_frame);
  va_enc_class->reorder_frame =
      GST_DEBUG_FUNCPTR (gst_va_av1_enc_reorder_frame);
  va_enc_class->encode_frame = GST_DEBUG_FUNCPTR (gst_va_av1_enc_encode_frame);
  va_enc_class->prepare_output =
      GST_DEBUG_FUNCPTR (gst_va_av1_enc_prepare_output);

  {
    display = gst_va_display_platform_new (va_enc_class->render_device_path);
    encoder = gst_va_encoder_new (display, va_enc_class->codec,
        va_enc_class->entrypoint);
    if (gst_va_encoder_get_rate_control_enum (encoder,
            vaav1enc_class->rate_control)) {
      g_snprintf (vaav1enc_class->rate_control_type_name,
          G_N_ELEMENTS (vaav1enc_class->rate_control_type_name) - 1,
          "GstVaEncoderRateControl_%" GST_FOURCC_FORMAT "%s_%s",
          GST_FOURCC_ARGS (va_enc_class->codec),
          (va_enc_class->entrypoint == VAEntrypointEncSliceLP) ? "_LP" : "",
          g_path_get_basename (va_enc_class->render_device_path));
      vaav1enc_class->rate_control_type =
          g_enum_register_static (vaav1enc_class->rate_control_type_name,
          vaav1enc_class->rate_control);
      gst_type_mark_as_plugin_api (vaav1enc_class->rate_control_type, 0);
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
   * GstVaAV1Enc:key-int-max:
   *
   * The maximal distance between two keyframes.
   */
  properties[PROP_KEYFRAME_INT] = g_param_spec_uint ("key-int-max",
      "Key frame maximal interval",
      "The maximal distance between two keyframes. It decides the size of GOP"
      " (0: auto-calculate)", 0, MAX_KEY_FRAME_INTERVAL, 60,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

  /**
   * GstVaAV1Enc:gf-group-size:
   *
   * The size of the golden frame group.
   */
  properties[PROP_GOLDEN_GROUP_SIZE] = g_param_spec_uint ("gf-group-size",
      "Golden frame group size",
      "The size of the golden frame group.",
      1, MAX_GF_GROUP_SIZE, MAX_GF_GROUP_SIZE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

  /**
   * GstVaAV1Enc:ref-frames:
   *
   * The number of reference frames.
   */
  properties[PROP_NUM_REF_FRAMES] = g_param_spec_uint ("ref-frames",
      "Number of Reference Frames",
      "Number of reference frames, including both the forward and the backward",
      0, 7, 7, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

  /**
   * GstVaAV1Enc:hierarchical-level:
   *
   * The hierarchical level for golden frame group.
   */
  properties[PROP_HIERARCHICAL_LEVEL] =
      g_param_spec_uint ("hierarchical-level", "The hierarchical level",
      "The hierarchical level for golden frame group. Setting to 1 disables "
      "all future reference", 1, HIGHEST_PYRAMID_LEVELS,
      HIGHEST_PYRAMID_LEVELS,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

  /**
   * GstVaAV1Enc:superblock-128x128:
   *
   * Enable the 128x128 superblock mode.
   */
  properties[PROP_128X128_SUPERBLOCK] =
      g_param_spec_boolean ("superblock-128x128", "128x128 superblock",
      "Enable the 128x128 superblock mode", FALSE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

  /**
   * GstVaAV1Enc:min-qp:
   *
   * The minimum quantizer value.
   */
  properties[PROP_MIN_QP] = g_param_spec_uint ("min-qp", "Minimum QP",
      "Minimum quantizer value for each frame", 0, 255, 0,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

  /**
   * GstVaAV1Enc:max-qp:
   *
   * The maximum quantizer value.
   */
  properties[PROP_MAX_QP] = g_param_spec_uint ("max-qp", "Maximum QP",
      "Maximum quantizer value for each frame", 1, 255, 255,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

  /**
   * GstVaAV1Enc:qp:
   *
   * The basic quantizer value for all frames.
   */
  properties[PROP_QP] = g_param_spec_uint ("qp", "The frame QP",
      "The basic quantizer value for all frames.", 0, 255, DEFAULT_BASE_QINDEX,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

  /**
   * GstVaAV1Enc:bitrate:
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
      0, 2000 * 1024, 0,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

  /**
   * GstVaAV1Enc:target-percentage:
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
      50, 100, 66,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

  /**
   * GstVaAV1Enc:cpb-size:
   *
   * The desired max CPB size in Kb (0: auto-calculate).
   */
  properties[PROP_CPB_SIZE] = g_param_spec_uint ("cpb-size",
      "max CPB size in Kb",
      "The desired max CPB size in Kb (0: auto-calculate)", 0, 2000 * 1024, 0,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

  /**
   * GstVaAV1Enc:target-usage:
   *
   * The target usage of the encoder. It controls and balances the encoding
   * speed and the encoding quality. The lower value has better quality but
   * slower speed, the higher value has faster speed but lower quality.
   */
  properties[PROP_TARGET_USAGE] = g_param_spec_uint ("target-usage",
      "target usage",
      "The target usage to control and balance the encoding speed/quality",
      1, 7, 4, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

  /**
   * GstVaAV1Enc:num-tile-cols:
   *
   * The number of tile columns when tile encoding is enabled.
   */
  properties[PROP_NUM_TILE_COLS] = g_param_spec_uint ("num-tile-cols",
      "number of tile columns",
      "The number of columns for tile encoding", 1, GST_AV1_MAX_TILE_COLS, 1,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

  /**
   * GstVaAV1Enc:num-tile-rows:
   *
   * The number of tile rows when tile encoding is enabled.
   */
  properties[PROP_NUM_TILE_ROWS] = g_param_spec_uint ("num-tile-rows",
      "number of tile rows",
      "The number of rows for tile encoding", 1, GST_AV1_MAX_TILE_ROWS, 1,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

  /**
   * GstVaAV1Enc:tile-groups:
   *
   * The number of tile groups for each frame.
   */
  properties[PROP_TILE_GROUPS] = g_param_spec_uint ("tile-groups",
      "Number of tile groups", "Number of tile groups for each frame",
      1, GST_AV1_MAX_TILE_COLS * GST_AV1_MAX_TILE_ROWS, 1,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

  /**
   * GstVaAV1Enc:mbbrc:
   *
   * Macroblock level bitrate control.
   * This is not compatible with Constant QP rate control.
   */
  properties[PROP_MBBRC] = g_param_spec_enum ("mbbrc",
      "Macroblock level Bitrate Control",
      "Macroblock level Bitrate Control. It is not compatible with CQP",
      GST_TYPE_VA_FEATURE, GST_VA_FEATURE_AUTO,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

  if (vaav1enc_class->rate_control_type > 0) {
    properties[PROP_RATE_CONTROL] = g_param_spec_enum ("rate-control",
        "rate control mode",
        "The desired rate control mode for the encoder",
        vaav1enc_class->rate_control_type,
        vaav1enc_class->rate_control[0].value,
        GST_PARAM_CONDITIONALLY_AVAILABLE | G_PARAM_READWRITE |
        G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);
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
  g_value_set_string (&val, "tu");
  gst_caps_set_value (caps, "alignment", &val);
  g_value_unset (&val);

  g_value_init (&val, G_TYPE_STRING);
  g_value_set_string (&val, "obu-stream");
  gst_caps_set_value (caps, "stream-format", &val);
  g_value_unset (&val);

  return caps;
}

gboolean
gst_va_av1_enc_register (GstPlugin * plugin, GstVaDevice * device,
    GstCaps * sink_caps, GstCaps * src_caps, guint rank,
    VAEntrypoint entrypoint)
{
  static GOnce debug_once = G_ONCE_INIT;
  GType type;
  GTypeInfo type_info = {
    .class_size = sizeof (GstVaAV1EncClass),
    .class_init = gst_va_av1_enc_class_init,
    .instance_size = sizeof (GstVaAV1Enc),
    .instance_init = gst_va_av1_enc_init,
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
    gst_va_create_feature_name (device, "GstVaAV1Enc", "GstVa%sAV1Enc",
        &type_name, "vaav1enc", "va%sav1enc", &feature_name,
        &cdata->description, &rank);
  } else {
    gst_va_create_feature_name (device, "GstVaAV1LPEnc", "GstVa%sAV1LPEnc",
        &type_name, "vaav1lpenc", "va%sav1lpenc", &feature_name,
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
