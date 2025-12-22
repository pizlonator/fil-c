/* GStreamer
 *  Copyright (C) 2021 Intel Corporation
 *     Author: He Junyan <junyan.he@intel.com>
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
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
 * SECTION:element-vah264enc
 * @title: vah264enc
 * @short_description: A VA-API based H264 video encoder
 *
 * vah264enc encodes raw video VA surfaces into H.264 bitstreams using
 * the installed and chosen [VA-API](https://01.org/linuxmedia/vaapi)
 * driver.
 *
 * The raw video frames in main memory can be imported into VA surfaces.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc num-buffers=60 ! timeoverlay ! vah264enc ! h264parse ! mp4mux ! filesink location=test.mp4
 * ```
 *
 * Since: 1.22
 *
 */

 /* @TODO:
  * 1. Look ahead, which can optimize the slice type and QP.
  * 2. Field encoding.
  * 3. The stereo encoding such as the frame-packing or MVC.
  * 4. Weight prediction of B frame.
  * 5. latency calculation.
  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvah264enc.h"

#include <gst/codecparsers/gsth264bitwriter.h>
#include <gst/va/gstva.h>
#include <gst/va/gstvavideoformat.h>
#include <gst/va/vasurfaceimage.h>
#include <gst/video/video.h>
#include <va/va_drmcommon.h>

#include "gstvabaseenc.h"
#include "gstvacaps.h"
#include "gstvadisplay_priv.h"
#include "gstvaencoder.h"
#include "gstvaprofile.h"
#include "vacompat.h"
#include "gstvapluginutils.h"

GST_DEBUG_CATEGORY_STATIC (gst_va_h264enc_debug);
#define GST_CAT_DEFAULT gst_va_h264enc_debug

#define GST_VA_H264_ENC(obj)            ((GstVaH264Enc *) obj)
#define GST_VA_H264_ENC_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_FROM_INSTANCE (obj), GstVaH264EncClass))
#define GST_VA_H264_ENC_CLASS(klass)    ((GstVaH264EncClass *) klass)

typedef struct _GstVaH264Enc GstVaH264Enc;
typedef struct _GstVaH264EncClass GstVaH264EncClass;
typedef struct _GstVaH264EncFrame GstVaH264EncFrame;
typedef struct _GstVaH264LevelLimits GstVaH264LevelLimits;

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
  PROP_DCT8X8,
  PROP_CABAC,
  PROP_TRELLIS,
  PROP_MBBRC,
  PROP_BITRATE,
  PROP_TARGET_PERCENTAGE,
  PROP_TARGET_USAGE,
  PROP_CPB_SIZE,
  PROP_AUD,
  PROP_CC,
  PROP_RATE_CONTROL,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

static GstElementClass *parent_class = NULL;

/* Scale factor for bitrate (HRD bit_rate_scale: min = 6) */
#define SX_BITRATE 6
/* Scale factor for CPB size (HRD cpb_size_scale: min = 4) */
#define SX_CPB_SIZE 4
/* Maximum sizes for common headers (in bits) */
#define MAX_SPS_HDR_SIZE  16473
#define MAX_VUI_PARAMS_SIZE  210
#define MAX_HRD_PARAMS_SIZE  4103
#define MAX_PPS_HDR_SIZE  101
#define MAX_SLICE_HDR_SIZE  397 + 2572 + 6670 + 2402

#define MAX_GOP_SIZE  1024

/* *INDENT-OFF* */
struct _GstVaH264EncClass
{
  GstVaBaseEncClass parent_class;

  GType rate_control_type;
  char rate_control_type_name[64];
  GEnumValue rate_control[16];
};
/* *INDENT-ON* */

struct _GstVaH264Enc
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
    gboolean use_cabac;
    gboolean use_dct8x8;
    gboolean use_trellis;
    gboolean aud;
    gboolean cc;
    guint32 mbbrc;
    guint32 num_slices;
    guint32 cpb_size;
    guint32 target_percentage;
    guint32 target_usage;
  } prop;

  /* H264 fields */
  gint mb_width;
  gint mb_height;
  guint8 level_idc;
  const gchar *level_str;
  /* Minimum Compression Ratio (A.3.1) */
  guint min_cr;
  gboolean use_cabac;
  gboolean use_dct8x8;
  gboolean use_trellis;
  gboolean aud;
  gboolean cc;
  guint32 num_slices;
  guint32 packed_headers;

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
    /* current index in the frames types map. */
    guint cur_frame_index;
    /* Number of ref frames within current GOP. H264's frame num. */
    gint cur_frame_num;
    /* Max frame num within a GOP. */
    guint32 max_frame_num;
    guint32 log2_max_frame_num;
    /* Max poc within a GOP. */
    guint32 max_pic_order_cnt;
    guint32 log2_max_pic_order_cnt;

    /* Total ref frames of list0 and list1. */
    guint32 num_ref_frames;
    guint32 ref_num_list0;
    guint32 ref_num_list1;

    guint num_reorder_frames;

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

  GstH264SPS sequence_hdr;
};

struct _GstVaH264EncFrame
{
  GstVaEncodePicture *picture;
  GstH264SliceType type;
  gboolean is_ref;
  guint pyramid_level;
  /* Only for b pyramid */
  gint left_ref_poc_diff;
  gint right_ref_poc_diff;

  gint poc;
  gint frame_num;
  /* The pic_num will be marked as unused_for_reference, which is
   * replaced by this frame. -1 if we do not need to care about it
   * explicitly. */
  gint unused_for_reference_pic_num;

  gboolean last_frame;
};

/**
 * GstVaH264LevelLimits:
 * @name: the level name
 * @level_idc: the H.264 level_idc value
 * @MaxMBPS: the maximum macroblock processing rate (MB/sec)
 * @MaxFS: the maximum frame size (MBs)
 * @MaxDpbMbs: the maxium decoded picture buffer size (MBs)
 * @MaxBR: the maximum video bit rate (kbps)
 * @MaxCPB: the maximum CPB size (kbits)
 * @MinCR: the minimum Compression Ratio
 *
 * The data structure that describes the limits of an H.264 level.
 */
struct _GstVaH264LevelLimits
{
  const gchar *name;
  guint8 level_idc;
  guint32 MaxMBPS;
  guint32 MaxFS;
  guint32 MaxDpbMbs;
  guint32 MaxBR;
  guint32 MaxCPB;
  guint32 MinCR;
};

/* Table A-1 - Level limits */
/* *INDENT-OFF* */
static const GstVaH264LevelLimits _va_h264_level_limits[] = {
  /* level   idc   MaxMBPS   MaxFS   MaxDpbMbs  MaxBR   MaxCPB  MinCr */
  {  "1",    GST_H264_LEVEL_L1,   1485,     99,     396,       64,     175,    2 },
  {  "1b",   GST_H264_LEVEL_L1B,   1485,     99,     396,       128,    350,    2 },
  {  "1.1",  GST_H264_LEVEL_L1_1,   3000,     396,    900,       192,    500,    2 },
  {  "1.2",  GST_H264_LEVEL_L1_2,   6000,     396,    2376,      384,    1000,   2 },
  {  "1.3",  GST_H264_LEVEL_L1_3,   11880,    396,    2376,      768,    2000,   2 },
  {  "2",    GST_H264_LEVEL_L2,   11880,    396,    2376,      2000,   2000,   2 },
  {  "2.1",  GST_H264_LEVEL_L2_1,   19800,    792,    4752,      4000,   4000,   2 },
  {  "2.2",  GST_H264_LEVEL_L2_2,   20250,    1620,   8100,      4000,   4000,   2 },
  {  "3",    GST_H264_LEVEL_L3,   40500,    1620,   8100,      10000,  10000,  2 },
  {  "3.1",  GST_H264_LEVEL_L3_1,   108000,   3600,   18000,     14000,  14000,  4 },
  {  "3.2",  GST_H264_LEVEL_L3_2,   216000,   5120,   20480,     20000,  20000,  4 },
  {  "4",    GST_H264_LEVEL_L4,   245760,   8192,   32768,     20000,  25000,  4 },
  {  "4.1",  GST_H264_LEVEL_L4_1,   245760,   8192,   32768,     50000,  62500,  2 },
  {  "4.2",  GST_H264_LEVEL_L4_2,   522240,   8704,   34816,     50000,  62500,  2 },
  {  "5",    GST_H264_LEVEL_L5,   589824,   22080,  110400,    135000, 135000, 2 },
  {  "5.1",  GST_H264_LEVEL_L5_1,   983040,   36864,  184320,    240000, 240000, 2 },
  {  "5.2",  GST_H264_LEVEL_L5_2,   2073600,  36864,  184320,    240000, 240000, 2 },
  {  "6",    GST_H264_LEVEL_L6,   4177920,  139264, 696320,    240000, 240000, 2 },
  {  "6.1",  GST_H264_LEVEL_L6_1,   8355840,  139264, 696320,    480000, 480000, 2 },
  {  "6.2",  GST_H264_LEVEL_L6_2,  16711680,  139264, 696320,    800000, 800000, 2 },
};
/* *INDENT-ON* */

#ifndef GST_DISABLE_GST_DEBUG
static const gchar *
_slice_type_name (GstH264SliceType type)
{
  switch (type) {
    case GST_H264_P_SLICE:
      return "P";
    case GST_H264_B_SLICE:
      return "B";
    case GST_H264_I_SLICE:
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
#endif

static GstVaH264EncFrame *
gst_va_enc_frame_new (void)
{
  GstVaH264EncFrame *frame;

  frame = g_new (GstVaH264EncFrame, 1);
  frame->frame_num = 0;
  frame->unused_for_reference_pic_num = -1;
  frame->picture = NULL;
  frame->last_frame = FALSE;

  return frame;
}

static void
gst_va_enc_frame_free (gpointer pframe)
{
  GstVaH264EncFrame *frame = pframe;
  g_clear_pointer (&frame->picture, gst_va_encode_picture_free);
  g_free (frame);
}

static inline GstVaH264EncFrame *
_enc_frame (GstVideoCodecFrame * frame)
{
  GstVaH264EncFrame *enc_frame = gst_video_codec_frame_get_user_data (frame);
  g_assert (enc_frame);
  return enc_frame;
}

/* Normalizes bitrate (and CPB size) for HRD conformance */
static void
_calculate_bitrate_hrd (GstVaH264Enc * self)
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

#define update_property(type, obj, old_val, new_val, prop_id)           \
  gst_va_base_enc_update_property_##type (obj, old_val, new_val, properties[prop_id])
#define update_property_uint(obj, old_val, new_val, prop_id)    \
  update_property (uint, obj, old_val, new_val, prop_id)
#define update_property_bool(obj, old_val, new_val, prop_id)    \
  update_property (bool, obj, old_val, new_val, prop_id)

/* Estimates a good enough bitrate if none was supplied */
static gboolean
_ensure_rate_control (GstVaH264Enc * self)
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
    /* Default compression: 48 bits per macroblock in "high-compression" mode */
    guint bits_per_mb = 48;
    guint64 factor;

    /* According to the literature and testing, CABAC entropy coding
     * mode could provide for +10% to +18% improvement in general,
     * thus estimating +15% here ; and using adaptive 8x8 transforms
     * in I-frames could bring up to +10% improvement. */
    if (!self->use_cabac)
      bits_per_mb += (bits_per_mb * 15) / 100;
    if (!self->use_dct8x8)
      bits_per_mb += (bits_per_mb * 10) / 100;

    factor = (guint64) self->mb_width * self->mb_height * bits_per_mb;
    bitrate = gst_util_uint64_scale (factor,
        GST_VIDEO_INFO_FPS_N (&base->in_info),
        GST_VIDEO_INFO_FPS_D (&base->in_info)) / 1000;
    GST_INFO_OBJECT (self, "target bitrate computed to %u kbps", bitrate);
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
    _calculate_bitrate_hrd (self);

  /* update & notifications */
  update_property_uint (base, &self->prop.bitrate, bitrate, PROP_BITRATE);
  update_property_uint (base, &self->prop.cpb_size, self->rc.cpb_size,
      PROP_CPB_SIZE);
  update_property_uint (base, &self->prop.target_percentage,
      self->rc.target_percentage, PROP_TARGET_PERCENTAGE);
  update_property_uint (base, &self->prop.qp_i, self->rc.qp_i, PROP_QP_I);
  update_property_uint (base, &self->prop.qp_p, self->rc.qp_p, PROP_QP_P);
  update_property_uint (base, &self->prop.qp_b, self->rc.qp_b, PROP_QP_B);

  return TRUE;
}

static guint
_get_h264_cpb_nal_factor (VAProfile profile)
{
  guint f;

  /* Table A-2 */
  switch (profile) {
    case VAProfileH264High:
      f = 1500;
      break;
    case VAProfileH264ConstrainedBaseline:
    case VAProfileH264Main:
      f = 1200;
      break;
    case VAProfileH264MultiviewHigh:
    case VAProfileH264StereoHigh:
      f = 1500;                 /* H.10.2.1 (r) */
      break;
    default:
      g_assert_not_reached ();
      f = 1200;
      break;
  }
  return f;
}

/* Derives the level from the currently set limits */
static gboolean
_calculate_level (GstVaH264Enc * self)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  const guint cpb_factor = _get_h264_cpb_nal_factor (base->profile);
  guint i, PicSizeMbs, MaxDpbMbs, MaxMBPS;

  PicSizeMbs = self->mb_width * self->mb_height;
  MaxDpbMbs = PicSizeMbs * (self->gop.num_ref_frames + 1);
  MaxMBPS = gst_util_uint64_scale_int_ceil (PicSizeMbs,
      GST_VIDEO_INFO_FPS_N (&base->in_info),
      GST_VIDEO_INFO_FPS_D (&base->in_info));

  for (i = 0; i < G_N_ELEMENTS (_va_h264_level_limits); i++) {
    const GstVaH264LevelLimits *const limits = &_va_h264_level_limits[i];
    if (PicSizeMbs <= limits->MaxFS && MaxDpbMbs <= limits->MaxDpbMbs
        && MaxMBPS <= limits->MaxMBPS && (!self->rc.max_bitrate_bits
            || self->rc.max_bitrate_bits <= (limits->MaxBR * 1000 * cpb_factor))
        && (!self->rc.cpb_length_bits
            || self->rc.cpb_length_bits <=
            (limits->MaxCPB * 1000 * cpb_factor))) {

      self->level_idc = _va_h264_level_limits[i].level_idc;
      self->level_str = _va_h264_level_limits[i].name;
      self->min_cr = _va_h264_level_limits[i].MinCR;

      return TRUE;
    }
  }

  GST_ERROR_OBJECT (self,
      "failed to find a suitable level matching codec config");
  return FALSE;
}

static void
_validate_parameters (GstVaH264Enc * self)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  gint32 max_slices;

  /* Ensure the num_slices provided by the user not exceed the limit
   * of the number of slices permitted by the stream and by the
   * hardware. */
  g_assert (self->num_slices >= 1);
  max_slices = gst_va_encoder_get_max_slice_num (base->encoder,
      base->profile, GST_VA_BASE_ENC_ENTRYPOINT (base));
  if (self->num_slices > max_slices)
    self->num_slices = max_slices;
  /* The stream size limit. */
  if (self->num_slices > ((self->mb_width * self->mb_height + 1) / 2))
    self->num_slices = ((self->mb_width * self->mb_height + 1) / 2);

  update_property_uint (base, &self->prop.num_slices,
      self->num_slices, PROP_NUM_SLICES);

  /* Ensure trellis. */
  if (self->use_trellis &&
      !gst_va_encoder_has_trellis (base->encoder, base->profile,
          GST_VA_BASE_ENC_ENTRYPOINT (base))) {
    GST_INFO_OBJECT (self, "The trellis is not supported");
    self->use_trellis = FALSE;
  }

  update_property_bool (base, &self->prop.use_trellis, self->use_trellis,
      PROP_TRELLIS);
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

static void
_print_gop_structure (GstVaH264Enc * self)
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

    g_string_append_printf (str, "%s",
        _slice_type_name (self->gop.frame_types[i].slice_type));

    if (self->gop.b_pyramid
        && self->gop.frame_types[i].slice_type == GST_H264_B_SLICE) {
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
      self->gop.ref_num_list0, self->gop.ref_num_list1, str->str);

  g_string_free (str, TRUE);
#endif
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
      info[index].left_ref_poc_diff = (index + 1) * -2;
      info[index].right_ref_poc_diff = (len - index) * 2;
    }

    return;
  }

  index = len / 2;
  info[index].level = current_level;
  info[index].left_ref_poc_diff = (index + 1) * -2;
  info[index].right_ref_poc_diff = (len - index) * 2;

  current_level++;

  if (index > 0)
    _set_pyramid_info (info, index, current_level, highest_level);

  if (index + 1 < len)
    _set_pyramid_info (&info[index + 1], len - (index + 1),
        current_level, highest_level);
}

static void
_create_gop_frame_types (GstVaH264Enc * self)
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
      self->gop.frame_types[i].slice_type = GST_H264_I_SLICE;
      self->gop.frame_types[i].is_ref = TRUE;
      continue;
    }

    /* Intra only stream. */
    if (self->gop.ip_period == 0) {
      self->gop.frame_types[i].slice_type = GST_H264_I_SLICE;
      self->gop.frame_types[i].is_ref = FALSE;
      continue;
    }

    if (i % self->gop.ip_period) {
      guint pyramid_index =
          i % self->gop.ip_period - 1 /* The first P or IDR */ ;

      self->gop.frame_types[i].slice_type = GST_H264_B_SLICE;
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
      self->gop.frame_types[i].slice_type = GST_H264_I_SLICE;
      self->gop.frame_types[i].is_ref = TRUE;
      i_frames--;
      continue;
    }

    self->gop.frame_types[i].slice_type = GST_H264_P_SLICE;
    self->gop.frame_types[i].is_ref = TRUE;
  }

  /* Force the last one to be a P */
  if (self->gop.idr_period > 1 && self->gop.ip_period > 0) {
    self->gop.frame_types[self->gop.idr_period - 1].slice_type =
        GST_H264_P_SLICE;
    self->gop.frame_types[self->gop.idr_period - 1].is_ref = TRUE;
  }
}

/* Consider the idr_period, num_bframes, L0/L1 reference number.
 * TODO: Load some preset fixed GOP structure.
 * TODO: Skip this if in lookahead mode. */
static void
_generate_gop_structure (GstVaH264Enc * self)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint32 list0, list1, gop_ref_num;
  gint32 p_frames;

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

  if (list0 > self->gop.num_ref_frames)
    list0 = self->gop.num_ref_frames;
  if (list1 > self->gop.num_ref_frames)
    list1 = self->gop.num_ref_frames;

  if (list0 == 0) {
    GST_INFO_OBJECT (self,
        "No reference support, fallback to intra only stream");

    /* It does not make sense that if only the list1 exists. */
    self->gop.num_ref_frames = 0;

    self->gop.ip_period = 0;
    self->gop.num_bframes = 0;
    self->gop.b_pyramid = FALSE;
    self->gop.highest_pyramid_level = 0;
    self->gop.num_iframes = self->gop.idr_period - 1 /* The idr */ ;
    self->gop.ref_num_list0 = 0;
    self->gop.ref_num_list1 = 0;
    goto create_poc;
  }

  if (self->gop.num_ref_frames <= 1) {
    GST_INFO_OBJECT (self, "The number of reference frames is only %d,"
        " no B frame allowed, fallback to I/P mode", self->gop.num_ref_frames);
    self->gop.num_bframes = 0;
    list1 = 0;
  }

  /* b_pyramid needs at least 1 ref for B, besides the I/P */
  if (self->gop.b_pyramid && self->gop.num_ref_frames <= 2) {
    GST_INFO_OBJECT (self, "The number of reference frames is only %d,"
        " not enough for b_pyramid", self->gop.num_ref_frames);
    self->gop.b_pyramid = FALSE;
  }

  if (list1 == 0 && self->gop.num_bframes > 0) {
    GST_INFO_OBJECT (self,
        "No hw reference support for list 1, fallback to I/P mode");
    self->gop.num_bframes = 0;
    self->gop.b_pyramid = FALSE;
  }

  /* I/P mode, no list1 needed. */
  if (self->gop.num_bframes == 0)
    list1 = 0;

  /* Not enough B frame, no need for b_pyramid. */
  if (self->gop.num_bframes <= 1)
    self->gop.b_pyramid = FALSE;

  /* b pyramid has only one backward ref. */
  if (self->gop.b_pyramid)
    list1 = 1;

  if (self->gop.num_ref_frames > list0 + list1) {
    self->gop.num_ref_frames = list0 + list1;
    GST_INFO_OBJECT (self, "HW limits, lowering the number of reference"
        " frames to %d", self->gop.num_ref_frames);
  }

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
    self->gop.ref_num_list0 = self->gop.num_ref_frames;
    self->gop.ref_num_list1 = 0;
  } else if (self->gop.b_pyramid) {
    guint b_frames = self->gop.num_bframes;
    guint b_refs;

    /* b pyramid has only one backward ref. */
    g_assert (list1 == 1);
    self->gop.ref_num_list1 = list1;
    self->gop.ref_num_list0 =
        self->gop.num_ref_frames - self->gop.ref_num_list1;

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
    self->gop.ref_num_list1 = 1;
    self->gop.ref_num_list0 =
        self->gop.num_ref_frames - self->gop.ref_num_list1;
    /* Balance the forward and backward refs, but not cause a big latency. */
    while ((self->gop.num_bframes * self->gop.ref_num_list1 <= 16)
        && (self->gop.ref_num_list1 <= gop_ref_num)
        && (self->gop.ref_num_list1 < list1)
        && (self->gop.ref_num_list0 / self->gop.ref_num_list1 > 4)) {
      self->gop.ref_num_list0--;
      self->gop.ref_num_list1++;
    }

    if (self->gop.ref_num_list0 > list0)
      self->gop.ref_num_list0 = list0;
  }

  /* It's OK, keep slots for GST_VIDEO_CODEC_FRAME_IS_FORCE_KEYFRAME frame. */
  if (self->gop.ref_num_list0 > gop_ref_num)
    GST_DEBUG_OBJECT (self, "num_ref_frames %d is bigger than gop_ref_num %d",
        self->gop.ref_num_list0, gop_ref_num);

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
  self->gop.log2_max_frame_num = _get_log2_max_num (self->gop.idr_period);
  self->gop.max_frame_num = (1 << self->gop.log2_max_frame_num);
  self->gop.log2_max_pic_order_cnt = self->gop.log2_max_frame_num + 1;
  self->gop.max_pic_order_cnt = (1 << self->gop.log2_max_pic_order_cnt);
  self->gop.num_reorder_frames = self->gop.b_pyramid ?
      self->gop.highest_pyramid_level * 2 + 1 /* the last P frame. */ :
      self->gop.ref_num_list1;
  /* Should not exceed the max ref num. */
  self->gop.num_reorder_frames =
      MIN (self->gop.num_reorder_frames, self->gop.num_ref_frames);
  self->gop.num_reorder_frames = MIN (self->gop.num_reorder_frames, 16);

  _create_gop_frame_types (self);
  _print_gop_structure (self);

  /* updates & notifications */
  update_property_uint (base, &self->prop.num_ref_frames,
      self->gop.num_ref_frames, PROP_NUM_REF_FRAMES);
  update_property_uint (base, &self->prop.num_iframes, self->gop.num_iframes,
      PROP_IFRAMES);
}

static void
_calculate_coded_size (GstVaH264Enc * self)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint codedbuf_size = 0;

  if (base->profile == VAProfileH264High
      || base->profile == VAProfileH264MultiviewHigh
      || base->profile == VAProfileH264StereoHigh) {
    /* The number of bits of macroblock_layer( ) data for any macroblock
       is not greater than 128 + RawMbBits */
    guint RawMbBits = 0;
    guint BitDepthY = 8;
    guint BitDepthC = 8;
    guint MbWidthC = 8;
    guint MbHeightC = 8;

    switch (base->rt_format) {
      case VA_RT_FORMAT_YUV420:
        BitDepthY = 8;
        BitDepthC = 8;
        MbWidthC = 8;
        MbHeightC = 8;
        break;
      case VA_RT_FORMAT_YUV422:
        BitDepthY = 8;
        BitDepthC = 8;
        MbWidthC = 8;
        MbHeightC = 16;
        break;
      case VA_RT_FORMAT_YUV444:
        BitDepthY = 8;
        BitDepthC = 8;
        MbWidthC = 16;
        MbHeightC = 16;
        break;
      case VA_RT_FORMAT_YUV400:
        BitDepthY = 8;
        BitDepthC = 0;
        MbWidthC = 0;
        MbHeightC = 0;
        break;
      case VA_RT_FORMAT_YUV420_10:
        BitDepthY = 10;
        BitDepthC = 10;
        MbWidthC = 8;
        MbHeightC = 8;
        break;
      case VA_RT_FORMAT_YUV422_10:
        BitDepthY = 10;
        BitDepthC = 10;
        MbWidthC = 8;
        MbHeightC = 16;
        break;
      case VA_RT_FORMAT_YUV444_10:
        BitDepthY = 10;
        BitDepthC = 10;
        MbWidthC = 16;
        MbHeightC = 16;
        break;
      default:
        g_assert_not_reached ();
        break;
    }

    /* The variable RawMbBits is derived as
     * RawMbBits = 256 * BitDepthY + 2 * MbWidthC * MbHeightC * BitDepthC */
    RawMbBits = 256 * BitDepthY + 2 * MbWidthC * MbHeightC * BitDepthC;
    codedbuf_size = (self->mb_width * self->mb_height) * (128 + RawMbBits) / 8;
  } else {
    /* The number of bits of macroblock_layer( ) data for any macroblock
     * is not greater than 3200 */
    codedbuf_size = (self->mb_width * self->mb_height) * (3200 / 8);
  }

  /* Account for SPS header */
  /* XXX: exclude scaling lists, MVC/SVC extensions */
  codedbuf_size += 4 /* start code */  + GST_ROUND_UP_8 (MAX_SPS_HDR_SIZE +
      MAX_VUI_PARAMS_SIZE + 2 * MAX_HRD_PARAMS_SIZE) / 8;

  /* Account for PPS header */
  /* XXX: exclude slice groups, scaling lists, MVC/SVC extensions */
  codedbuf_size += 4 + GST_ROUND_UP_8 (MAX_PPS_HDR_SIZE) / 8;

  /* Account for slice header */
  codedbuf_size +=
      self->num_slices * (4 + GST_ROUND_UP_8 (MAX_SLICE_HDR_SIZE) / 8);

  /* Add 5% for safety */
  base->codedbuf_size = (guint) ((gfloat) codedbuf_size * 1.05);

  GST_DEBUG_OBJECT (self, "Calculate codedbuf size: %u", base->codedbuf_size);
}

static guint
_get_rtformat (GstVaH264Enc * self, GstVideoFormat format)
{
  guint chroma;

  chroma = gst_va_chroma_from_video_format (format);

  /* Check whether the rtformat is supported. */
  if (chroma != VA_RT_FORMAT_YUV420) {
    GST_ERROR_OBJECT (self, "Unsupported chroma for video format: %s",
        gst_video_format_to_string (format));
    return 0;
  }

  return chroma;
}

static gboolean
_init_packed_headers (GstVaH264Enc * self)
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


static gboolean
_decide_profile (GstVaH264Enc * self, VAProfile * _profile, guint * _rt_format)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  gboolean ret = FALSE;
  GstVideoFormat in_format;
  VAProfile profile;
  guint rt_format;
  GstCaps *allowed_caps = NULL;
  guint num_structures, i;
  GstStructure *structure;
  const GValue *v_profile;
  GPtrArray *candidates = NULL;
  gchar *profile_name;

  candidates = g_ptr_array_new_with_free_func (g_free);

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
        profile_name = g_strdup (g_value_get_string (v_profile));
        g_ptr_array_add (candidates, profile_name);
      } else if (GST_VALUE_HOLDS_LIST (v_profile)) {
        guint j;

        for (j = 0; j < gst_value_list_get_size (v_profile); j++) {
          const GValue *p = gst_value_list_get_value (v_profile, j);
          if (!p)
            continue;

          profile_name = g_strdup (g_value_get_string (p));
          g_ptr_array_add (candidates, profile_name);
        }
      }
    }
  }

  if (candidates->len == 0) {
    GST_ERROR_OBJECT (self, "No available profile in caps");
    ret = FALSE;
    goto out;
  }

  in_format = GST_VIDEO_INFO_FORMAT (&base->in_info);
  rt_format = _get_rtformat (self, in_format);
  if (!rt_format) {
    GST_ERROR_OBJECT (self, "unsupported video format %s",
        gst_video_format_to_string (in_format));
    ret = FALSE;
    goto out;
  }

  /* Find the suitable profile by features and check the HW
   * support. */
  ret = FALSE;
  for (i = 0; i < candidates->len; i++) {
    profile_name = g_ptr_array_index (candidates, i);

    /* dct8x8 require at least high profile. */
    if (self->use_dct8x8) {
      if (!g_strstr_len (profile_name, -1, "high"))
        continue;
    }

    /* cabac require at least main profile. */
    if (self->use_cabac) {
      if (!g_strstr_len (profile_name, -1, "main")
          && !g_strstr_len (profile_name, -1, "high"))
        continue;
    }

    /* baseline only support I/P mode. */
    if (self->gop.num_bframes > 0) {
      if (g_strstr_len (profile_name, -1, "baseline"))
        continue;
    }

    profile = gst_va_profile_from_name (H264, profile_name);
    if (profile == VAProfileNone)
      continue;

    if (!gst_va_encoder_has_profile (base->encoder, profile))
      continue;

    if ((rt_format & gst_va_encoder_get_rtformat (base->encoder,
                profile, GST_VA_BASE_ENC_ENTRYPOINT (base))) == 0)
      continue;

    *_profile = profile;
    *_rt_format = rt_format;
    ret = TRUE;
    goto out;
  }

  /* Just use the first HW available profile and disable features if
   * needed. */
  profile_name = NULL;
  for (i = 0; i < candidates->len; i++) {
    profile_name = g_ptr_array_index (candidates, i);
    profile = gst_va_profile_from_name (H264, profile_name);
    if (profile == VAProfileNone)
      continue;

    if (!gst_va_encoder_has_profile (base->encoder, profile))
      continue;

    if ((rt_format & gst_va_encoder_get_rtformat (base->encoder,
                profile, GST_VA_BASE_ENC_ENTRYPOINT (base))) == 0)
      continue;

    *_profile = profile;
    *_rt_format = rt_format;
    ret = TRUE;
  }

  if (ret == FALSE)
    goto out;

  if (self->use_dct8x8 && !g_strstr_len (profile_name, -1, "high")) {
    GST_INFO_OBJECT (self, "Disable dct8x8, profile %s does not support it",
        gst_va_profile_name (profile));
    self->use_dct8x8 = FALSE;
    update_property_bool (base, &self->prop.use_dct8x8, self->use_dct8x8,
        PROP_DCT8X8);
  }

  if (self->use_cabac && (!g_strstr_len (profile_name, -1, "main")
          && !g_strstr_len (profile_name, -1, "high"))) {
    GST_INFO_OBJECT (self, "Disable cabac, profile %s does not support it",
        gst_va_profile_name (profile));
    self->use_cabac = FALSE;
    update_property_bool (base, &self->prop.use_cabac, self->use_cabac,
        PROP_CABAC);
  }

  if (self->gop.num_bframes > 0 && g_strstr_len (profile_name, -1, "baseline")) {
    GST_INFO_OBJECT (self, "No B frames, profile %s does not support it",
        gst_va_profile_name (profile));
    self->gop.num_bframes = 0;
    self->gop.b_pyramid = 0;
  }

out:
  g_clear_pointer (&candidates, g_ptr_array_unref);
  g_clear_pointer (&allowed_caps, gst_caps_unref);

  if (ret) {
    GST_INFO_OBJECT (self, "Select the profile %s",
        gst_va_profile_name (profile));
  } else {
    GST_ERROR_OBJECT (self, "Failed to find an available profile");
  }

  return ret;
}

/* Clear all the info of last reconfig and set the fields based on
 * property. The reconfig may change these fields because of the
 * profile/level and HW limitation. */
static void
gst_va_h264_enc_reset_state (GstVaBaseEnc * base)
{
  GstVaH264Enc *self = GST_VA_H264_ENC (base);

  GST_VA_BASE_ENC_CLASS (parent_class)->reset_state (base);

  GST_OBJECT_LOCK (self);
  self->use_cabac = self->prop.use_cabac;
  self->use_dct8x8 = self->prop.use_dct8x8;
  self->use_trellis = self->prop.use_trellis;
  self->aud = self->prop.aud;
  self->cc = self->prop.cc;
  self->num_slices = self->prop.num_slices;

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
  self->mb_width = 0;
  self->mb_height = 0;

  self->gop.i_period = 0;
  self->gop.total_idr_count = 0;
  self->gop.ip_period = 0;
  self->gop.highest_pyramid_level = 0;
  memset (self->gop.frame_types, 0, sizeof (self->gop.frame_types));
  self->gop.cur_frame_index = 0;
  self->gop.cur_frame_num = 0;
  self->gop.max_frame_num = 0;
  self->gop.log2_max_frame_num = 0;
  self->gop.max_pic_order_cnt = 0;
  self->gop.log2_max_pic_order_cnt = 0;
  self->gop.ref_num_list0 = 0;
  self->gop.ref_num_list1 = 0;
  self->gop.num_reorder_frames = 0;
  self->gop.last_keyframe = NULL;

  self->rc.max_bitrate = 0;
  self->rc.target_bitrate = 0;
  self->rc.max_bitrate_bits = 0;
  self->rc.target_bitrate_bits = 0;
  self->rc.cpb_length_bits = 0;

  memset (&self->sequence_hdr, 0, sizeof (GstH264SPS));
}

static gboolean
gst_va_h264_enc_reconfig (GstVaBaseEnc * base)
{
  GstVaBaseEncClass *klass = GST_VA_BASE_ENC_GET_CLASS (base);
  GstVideoEncoder *venc = GST_VIDEO_ENCODER (base);
  GstVaH264Enc *self = GST_VA_H264_ENC (base);
  GstCaps *out_caps, *reconf_caps = NULL;
  GstVideoCodecState *output_state = NULL;
  GstVideoFormat format, reconf_format = GST_VIDEO_FORMAT_UNKNOWN;
  VAProfile profile = VAProfileNone;
  gboolean do_renegotiation = TRUE, do_reopen, need_negotiation;
  guint max_ref_frames, max_surfaces = 0, rt_format = 0, codedbuf_size;
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

  if (!_decide_profile (self, &profile, &rt_format))
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

  self->mb_width = GST_ROUND_UP_16 (base->width) / 16;
  self->mb_height = GST_ROUND_UP_16 (base->height) / 16;

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

  GST_DEBUG_OBJECT (self, "resolution:%dx%d, MB size: %dx%d,"
      " frame duration is %" GST_TIME_FORMAT,
      base->width, base->height, self->mb_width, self->mb_height,
      GST_TIME_ARGS (base->frame_duration));

  _validate_parameters (self);

  if (!_ensure_rate_control (self))
    return FALSE;

  if (!_calculate_level (self))
    return FALSE;

  _generate_gop_structure (self);

  _calculate_coded_size (self);

  /* updates & notifications */
  /* num_bframes are modified several times before */
  update_property_uint (base, &self->prop.num_bframes, self->gop.num_bframes,
      PROP_BFRAMES);
  update_property_bool (base, &self->prop.b_pyramid, self->gop.b_pyramid,
      PROP_B_PYRAMID);

  if (!_init_packed_headers (self))
    return FALSE;

  self->aud = self->aud && self->packed_headers & VA_ENC_PACKED_HEADER_RAW_DATA;
  update_property_bool (base, &self->prop.aud, self->aud, PROP_AUD);

  self->cc = self->cc && self->packed_headers & VA_ENC_PACKED_HEADER_RAW_DATA;
  update_property_bool (base, &self->prop.cc, self->cc, PROP_CC);

  max_ref_frames = self->gop.num_ref_frames + 3 /* scratch frames */ ;

  /* second check after calculations */
  do_reopen |=
      !(max_ref_frames == max_surfaces && codedbuf_size == base->codedbuf_size);
  if (do_reopen && gst_va_encoder_is_open (base->encoder))
    gst_va_encoder_close (base->encoder);

  if (!gst_va_encoder_is_open (base->encoder)
      && !gst_va_encoder_open (base->encoder, base->profile,
          format, base->rt_format, base->width, base->height,
          base->codedbuf_size, max_ref_frames, self->rc.rc_ctrl_mode,
          self->packed_headers)) {
    GST_ERROR_OBJECT (self, "Failed to open the VA encoder.");
    return FALSE;
  }

  /* Add some tags */
  gst_va_base_enc_add_codec_tag (base, "H264");

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
_push_one_frame (GstVaBaseEnc * base, GstVideoCodecFrame * gst_frame,
    gboolean last)
{
  GstVaH264Enc *self = GST_VA_H264_ENC (base);
  GstVaH264EncFrame *frame;
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
        self->gop.cur_frame_num = 0;
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
        self->gop.cur_frame_num = 0;
      }

      frame->poc =
          ((self->gop.cur_frame_index * 2) % self->gop.max_pic_order_cnt);

      /* TODO: move most this logic onto vabaseenc class  */
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
          _slice_type_name (frame->type));

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
      self->gop.cur_frame_num = 0;
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
      if (frame->type == GST_H264_B_SLICE) {
        frame->type = GST_H264_P_SLICE;
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
  GstVaH264EncFrame *frame = _enc_frame (data);
  struct RefFramesCount *count = (struct RefFramesCount *) user_data;

  g_assert (frame->poc != count->poc);
  if (frame->poc > count->poc)
    count->num++;
}

static GstVideoCodecFrame *
_pop_pyramid_b_frame (GstVaH264Enc * self, guint gop_len)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint i;
  gint index = -1;
  GstVaH264EncFrame *b_vaframe;
  GstVideoCodecFrame *b_frame;
  struct RefFramesCount count;

  g_assert (self->gop.ref_num_list1 == 1);

  b_frame = NULL;
  b_vaframe = NULL;

  /* Find the lowest level with smallest poc. */
  for (i = 0; i < gop_len; i++) {
    GstVaH264EncFrame *vaf;
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
    GstVaH264EncFrame *vaf;
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

  /* Ensure we already have enough backward refs */
  count.num = 0;
  count.poc = b_vaframe->poc;
  g_queue_foreach (&base->ref_list, (GFunc) _count_backward_ref_num, &count);
  if (count.num >= self->gop.ref_num_list1) {
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
_pop_one_frame (GstVaBaseEnc * base, GstVideoCodecFrame ** out_frame)
{
  GstVaH264Enc *self = GST_VA_H264_ENC (base);
  GstVaH264EncFrame *vaframe;
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
  if (vaframe->type != GST_H264_B_SLICE) {
    frame = g_queue_pop_nth (&base->reorder_list, gop_len - 1);
    goto get_one;
  }

  if (self->gop.b_pyramid) {
    frame = _pop_pyramid_b_frame (self, gop_len);
    if (frame == NULL)
      return TRUE;
    goto get_one;
  }

  g_assert (self->gop.ref_num_list1 > 0);

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
  if (count.num >= self->gop.ref_num_list1) {
    frame = g_queue_pop_head (&base->reorder_list);
    goto get_one;
  }

  return TRUE;

get_one:
  g_assert (self->gop.cur_frame_num < self->gop.max_frame_num);

  vaframe = _enc_frame (frame);
  vaframe->frame_num = self->gop.cur_frame_num;

  /* Add the frame number for ref frames. */
  if (vaframe->is_ref)
    self->gop.cur_frame_num++;

  if (vaframe->frame_num == 0)
    self->gop.total_idr_count++;

  if (self->gop.b_pyramid && vaframe->type == GST_H264_B_SLICE) {
    GST_LOG_OBJECT (self, "pop a pyramid B frame with system_frame_number:"
        " %d, poc: %d, frame num: %d, is_ref: %s, level %d",
        frame->system_frame_number, vaframe->poc, vaframe->frame_num,
        vaframe->is_ref ? "true" : "false", vaframe->pyramid_level);
  } else {
    GST_LOG_OBJECT (self, "pop a frame with system_frame_number: %d,"
        " frame type: %s, poc: %d, frame num: %d, is_ref: %s",
        frame->system_frame_number, _slice_type_name (vaframe->type),
        vaframe->poc, vaframe->frame_num, vaframe->is_ref ? "true" : "false");
  }

  /* unref frame popped from queue or pyramid b_frame */
  gst_video_codec_frame_unref (frame);
  *out_frame = frame;
  return TRUE;
}

static gboolean
gst_va_h264_enc_reorder_frame (GstVaBaseEnc * base, GstVideoCodecFrame * frame,
    gboolean bump_all, GstVideoCodecFrame ** out_frame)
{
  if (!_push_one_frame (base, frame, bump_all)) {
    GST_ERROR_OBJECT (base, "Failed to push the input frame"
        " system_frame_number: %d into the reorder list",
        frame->system_frame_number);

    *out_frame = NULL;
    return FALSE;
  }

  if (!_pop_one_frame (base, out_frame)) {
    GST_ERROR_OBJECT (base, "Failed to pop the frame from the reorder list");
    *out_frame = NULL;
    return FALSE;
  }

  return TRUE;
}

static inline gboolean
_fill_sps (GstVaH264Enc * self, VAEncSequenceParameterBufferH264 * seq_param)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  GstH264Profile profile;
  guint32 constraint_set0_flag, constraint_set1_flag;
  guint32 constraint_set2_flag, constraint_set3_flag;
  guint32 max_dec_frame_buffering;

  /* let max_num_ref_frames <= MaxDpbFrames. */
  max_dec_frame_buffering =
      MIN (self->gop.num_ref_frames + 1 /* Last frame before bump */ ,
      16 /* DPB_MAX_SIZE */ );

  constraint_set0_flag = 0;
  constraint_set1_flag = 0;
  constraint_set2_flag = 0;
  constraint_set3_flag = 0;

  switch (base->profile) {
    case VAProfileH264ConstrainedBaseline:
      profile = GST_H264_PROFILE_BASELINE;
      /* A.2.1 (baseline profile constraints) */
      constraint_set0_flag = 1;
      constraint_set1_flag = 1;
      break;
    case VAProfileH264Main:
      profile = GST_H264_PROFILE_MAIN;
      /* A.2.2 (main profile constraints) */
      constraint_set1_flag = 1;
      break;
    case VAProfileH264High:
    case VAProfileH264MultiviewHigh:
    case VAProfileH264StereoHigh:
      profile = GST_H264_PROFILE_HIGH;
      break;
    default:
      return FALSE;
  }

  /* seq_scaling_matrix_present_flag not supported now */
  g_assert (seq_param->seq_fields.bits.seq_scaling_matrix_present_flag == 0);
  /* pic_order_cnt_type only support 0 now */
  g_assert (seq_param->seq_fields.bits.pic_order_cnt_type == 0);
  /* only progressive frames encoding is supported now */
  g_assert (seq_param->seq_fields.bits.frame_mbs_only_flag);

  /* *INDENT-OFF* */
  GST_DEBUG_OBJECT (self, "filling SPS");
  self->sequence_hdr = (GstH264SPS) {
    .id = 0,
    .profile_idc = profile,
    .constraint_set0_flag = constraint_set0_flag,
    .constraint_set1_flag = constraint_set1_flag,
    .constraint_set2_flag = constraint_set2_flag,
    .constraint_set3_flag = constraint_set3_flag,
    .level_idc = self->level_idc,

    .chroma_format_idc = seq_param->seq_fields.bits.chroma_format_idc,
    .bit_depth_luma_minus8 = seq_param->bit_depth_luma_minus8,
    .bit_depth_chroma_minus8 = seq_param->bit_depth_chroma_minus8,

    .log2_max_frame_num_minus4 =
        seq_param->seq_fields.bits.log2_max_frame_num_minus4,
    .pic_order_cnt_type = seq_param->seq_fields.bits.pic_order_cnt_type,
    .log2_max_pic_order_cnt_lsb_minus4 =
        seq_param->seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4,

    .num_ref_frames = seq_param->max_num_ref_frames,
    .gaps_in_frame_num_value_allowed_flag = 0,
    .pic_width_in_mbs_minus1 = seq_param->picture_width_in_mbs - 1,
    .pic_height_in_map_units_minus1 =
        (seq_param->seq_fields.bits.frame_mbs_only_flag ?
            seq_param->picture_height_in_mbs - 1 :
            seq_param->picture_height_in_mbs / 2 - 1),
    .frame_mbs_only_flag = seq_param->seq_fields.bits.frame_mbs_only_flag,
    .mb_adaptive_frame_field_flag = 0,
    .direct_8x8_inference_flag =
        seq_param->seq_fields.bits.direct_8x8_inference_flag,
    .frame_cropping_flag = seq_param->frame_cropping_flag,
    .frame_crop_left_offset = seq_param->frame_crop_left_offset,
    .frame_crop_right_offset = seq_param->frame_crop_right_offset,
    .frame_crop_top_offset = seq_param->frame_crop_top_offset,
    .frame_crop_bottom_offset = seq_param->frame_crop_bottom_offset,

    .vui_parameters_present_flag = seq_param->vui_parameters_present_flag,
    .vui_parameters = {
      .aspect_ratio_info_present_flag =
          seq_param->vui_fields.bits.aspect_ratio_info_present_flag,
      .aspect_ratio_idc = seq_param->aspect_ratio_idc,
      .sar_width = seq_param->sar_width,
      .sar_height = seq_param->sar_height,
      .overscan_info_present_flag = 0,
      .overscan_appropriate_flag = 0,
      .chroma_loc_info_present_flag = 0,
      .timing_info_present_flag =
          seq_param->vui_fields.bits.timing_info_present_flag,
      .num_units_in_tick = seq_param->num_units_in_tick,
      .time_scale = seq_param->time_scale,
      .fixed_frame_rate_flag = seq_param->vui_fields.bits.fixed_frame_rate_flag,

      /* We do not write hrd and no need for buffering period SEI. */
      .nal_hrd_parameters_present_flag = 0,
      .vcl_hrd_parameters_present_flag = 0,

      .low_delay_hrd_flag = seq_param->vui_fields.bits.low_delay_hrd_flag,
      .pic_struct_present_flag = 1,
      .bitstream_restriction_flag =
          seq_param->vui_fields.bits.bitstream_restriction_flag,
      .motion_vectors_over_pic_boundaries_flag =
          seq_param->vui_fields.bits.motion_vectors_over_pic_boundaries_flag,
      .max_bytes_per_pic_denom = 2,
      .max_bits_per_mb_denom = 1,
      .log2_max_mv_length_horizontal =
          seq_param->vui_fields.bits.log2_max_mv_length_horizontal,
      .log2_max_mv_length_vertical =
          seq_param->vui_fields.bits.log2_max_mv_length_vertical,
      .num_reorder_frames = self->gop.num_reorder_frames,
      .max_dec_frame_buffering = max_dec_frame_buffering,
    },
  };
  /* *INDENT-ON* */

  return TRUE;
}

static gboolean
_add_sequence_header (GstVaH264Enc * self, GstVaH264EncFrame * frame)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint size;
#define SPS_SIZE 4 + GST_ROUND_UP_8 (MAX_SPS_HDR_SIZE + MAX_VUI_PARAMS_SIZE + \
    2 * MAX_HRD_PARAMS_SIZE) / 8
  guint8 packed_sps[SPS_SIZE] = { 0, };
#undef SPS_SIZE

  size = sizeof (packed_sps);
  if (gst_h264_bit_writer_sps (&self->sequence_hdr, TRUE, packed_sps,
          &size) != GST_H264_BIT_WRITER_OK) {
    GST_ERROR_OBJECT (self, "Failed to generate the sequence header");
    return FALSE;
  }

  if (!gst_va_encoder_add_packed_header (base->encoder, frame->picture,
          VAEncPackedHeaderSequence, packed_sps, size * 8, FALSE)) {
    GST_ERROR_OBJECT (self, "Failed to add the packed sequence header");
    return FALSE;
  }

  return TRUE;
}

static inline void
_fill_sequence_param (GstVaH264Enc * self,
    VAEncSequenceParameterBufferH264 * sequence)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  gboolean direct_8x8_inference_flag = TRUE;

  g_assert (self->gop.log2_max_frame_num >= 4);
  g_assert (self->gop.log2_max_pic_order_cnt >= 4);

  /* A.2.3 Extended profile:
   * Sequence parameter sets shall have direct_8x8_inference_flag
   * equal to 1.
   *
   * A.3.3 Profile-specific level limits:
   * direct_8x8_inference_flag is not relevant to the Baseline,
   * Constrained Baseline, Constrained High, High 10 Intra, High 4:2:2
   * Intra, High 4:4:4 Intra, and CAVLC 4:4:4 Intra profiles as these
   * profiles do not allow B slice types, and
   * direct_8x8_inference_flag is equal to 1 for all levels of the
   * Extended profile. Table A-4.  We only have constrained baseline
   * here. */
  if (base->profile == VAProfileH264ConstrainedBaseline)
    direct_8x8_inference_flag = FALSE;

  /* *INDENT-OFF* */
  *sequence = (VAEncSequenceParameterBufferH264) {
    .seq_parameter_set_id = 0,
    .level_idc = self->level_idc,
    .intra_period =
        self->gop.i_period > 0 ? self->gop.i_period : self->gop.idr_period,
    .intra_idr_period = self->gop.idr_period,
    .ip_period = self->gop.ip_period,
    .bits_per_second = self->rc.target_bitrate_bits,
    .max_num_ref_frames = self->gop.num_ref_frames,
    .picture_width_in_mbs = self->mb_width,
    .picture_height_in_mbs = self->mb_height,

    .seq_fields.bits = {
      /* Only support 4:2:0 now. */
      .chroma_format_idc = 1,
      .frame_mbs_only_flag = 1,
      .mb_adaptive_frame_field_flag = FALSE,
      .seq_scaling_matrix_present_flag = FALSE,
      .direct_8x8_inference_flag = direct_8x8_inference_flag,
      .log2_max_frame_num_minus4 = self->gop.log2_max_frame_num - 4,
      .pic_order_cnt_type = 0,
      .log2_max_pic_order_cnt_lsb_minus4 = self->gop.log2_max_pic_order_cnt - 4,
    },
    .bit_depth_luma_minus8 = 0,
    .bit_depth_chroma_minus8 = 0,

    .vui_parameters_present_flag = TRUE,
    .vui_fields.bits = {
      .aspect_ratio_info_present_flag = TRUE,
      .timing_info_present_flag = TRUE,
      .bitstream_restriction_flag = TRUE,
      .log2_max_mv_length_horizontal = 15,
      .log2_max_mv_length_vertical = 15,
      .fixed_frame_rate_flag = 1,
      .low_delay_hrd_flag = 0,
      .motion_vectors_over_pic_boundaries_flag = TRUE,
    },
    .aspect_ratio_idc = 0xff,
    /* FIXME: what if no framerate info is provided */
    .sar_width = GST_VIDEO_INFO_PAR_N (&base->in_info),
    .sar_height = GST_VIDEO_INFO_PAR_D (&base->in_info),
    .num_units_in_tick = GST_VIDEO_INFO_FPS_D (&base->in_info),
    .time_scale = GST_VIDEO_INFO_FPS_N (&base->in_info) * 2,
  };
  /* *INDENT-ON* */

  /* frame_cropping_flag */
  if (base->width & 15 || base->height & 15) {
    static const guint SubWidthC[] = { 1, 2, 2, 1 };
    static const guint SubHeightC[] = { 1, 2, 1, 1 };
    const guint CropUnitX =
        SubWidthC[sequence->seq_fields.bits.chroma_format_idc];
    const guint CropUnitY =
        SubHeightC[sequence->seq_fields.bits.chroma_format_idc] *
        (2 - sequence->seq_fields.bits.frame_mbs_only_flag);

    sequence->frame_cropping_flag = 1;
    sequence->frame_crop_left_offset = 0;
    sequence->frame_crop_right_offset = (16 * self->mb_width -
        base->width) / CropUnitX;
    sequence->frame_crop_top_offset = 0;
    sequence->frame_crop_bottom_offset = (16 * self->mb_height -
        base->height) / CropUnitY;
  }
}

static gboolean
_add_sequence_parameter (GstVaH264Enc * self, GstVaEncodePicture * picture,
    VAEncSequenceParameterBufferH264 * sequence)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);

  if (!gst_va_encoder_add_param (base->encoder, picture,
          VAEncSequenceParameterBufferType, sequence, sizeof (*sequence))) {
    GST_ERROR_OBJECT (self, "Failed to create the sequence parameter");
    return FALSE;
  }

  return TRUE;
}

static inline gboolean
_fill_picture_parameter (GstVaH264Enc * self, GstVaH264EncFrame * frame,
    VAEncPictureParameterBufferH264 * pic_param)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint i;

  /* *INDENT-OFF* */
  *pic_param = (VAEncPictureParameterBufferH264) {
    .CurrPic = {
      .picture_id =
          gst_va_encode_picture_get_reconstruct_surface (frame->picture),
      .TopFieldOrderCnt = frame->poc,
    },
    .coded_buf = frame->picture->coded_buffer,
    /* Only support one sps and pps now. */
    .pic_parameter_set_id = 0,
    .seq_parameter_set_id = 0,
    /* means last encoding picture, EOS nal added. */
    .last_picture = frame->last_frame,
    .frame_num = frame->frame_num,

    .pic_init_qp = self->rc.qp_i,
    /* Use slice's these fields to control ref num. */
    .num_ref_idx_l0_active_minus1 = 0,
    .num_ref_idx_l1_active_minus1 = 0,
    .chroma_qp_index_offset = 0,
    .second_chroma_qp_index_offset = 0,
    /* picture fields */
    .pic_fields.bits.idr_pic_flag = (frame->frame_num == 0),
    .pic_fields.bits.reference_pic_flag = frame->is_ref,
    .pic_fields.bits.entropy_coding_mode_flag = self->use_cabac,
    .pic_fields.bits.weighted_pred_flag = 0,
    .pic_fields.bits.weighted_bipred_idc = 0,
    .pic_fields.bits.constrained_intra_pred_flag = 0,
    .pic_fields.bits.transform_8x8_mode_flag = self->use_dct8x8,
    /* enable debloking */
    .pic_fields.bits.deblocking_filter_control_present_flag = 1,
    .pic_fields.bits.redundant_pic_cnt_present_flag = 0,
    /* bottom_field_pic_order_in_frame_present_flag */
    .pic_fields.bits.pic_order_present_flag = 0,
    .pic_fields.bits.pic_scaling_matrix_present_flag = 0,
  };
  /* *INDENT-ON* */

  /* Non I frame, construct reference list. */
  i = 0;
  if (frame->type != GST_H264_I_SLICE) {
    GstVaH264EncFrame *f;

    if (g_queue_is_empty (&base->ref_list)) {
      GST_ERROR_OBJECT (self, "No reference found for frame type %s",
          _slice_type_name (frame->type));
      return FALSE;
    }

    g_assert (g_queue_get_length (&base->ref_list) <= self->gop.num_ref_frames);

    /* ref frames in queue are already sorted by frame_num. */
    for (; i < g_queue_get_length (&base->ref_list); i++) {
      f = _enc_frame (g_queue_peek_nth (&base->ref_list, i));

      pic_param->ReferenceFrames[i].picture_id =
          gst_va_encode_picture_get_reconstruct_surface (f->picture);
      pic_param->ReferenceFrames[i].TopFieldOrderCnt = f->poc;
      pic_param->ReferenceFrames[i].flags =
          VA_PICTURE_H264_SHORT_TERM_REFERENCE;
      pic_param->ReferenceFrames[i].frame_idx = f->frame_num;
    }
  }
  for (; i < 16; ++i)
    pic_param->ReferenceFrames[i].picture_id = VA_INVALID_ID;

  return TRUE;
};

static gboolean
_add_picture_parameter (GstVaH264Enc * self, GstVaH264EncFrame * frame,
    VAEncPictureParameterBufferH264 * pic_param)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);

  if (!gst_va_encoder_add_param (base->encoder, frame->picture,
          VAEncPictureParameterBufferType, pic_param,
          sizeof (VAEncPictureParameterBufferH264))) {
    GST_ERROR_OBJECT (self, "Failed to create the picture parameter");
    return FALSE;
  }

  return TRUE;
}

static void
_fill_pps (VAEncPictureParameterBufferH264 * pic_param, GstH264SPS * sps,
    GstH264PPS * pps)
{
  /* *INDENT-OFF* */
  *pps = (GstH264PPS) {
    .id = 0,
    .sequence = sps,
    .entropy_coding_mode_flag =
        pic_param->pic_fields.bits.entropy_coding_mode_flag,
    .pic_order_present_flag =
        pic_param->pic_fields.bits.pic_order_present_flag,
    .num_slice_groups_minus1 = 0,

    .num_ref_idx_l0_active_minus1 = pic_param->num_ref_idx_l0_active_minus1,
    .num_ref_idx_l1_active_minus1 = pic_param->num_ref_idx_l1_active_minus1,

    .weighted_pred_flag = pic_param->pic_fields.bits.weighted_pred_flag,
    .weighted_bipred_idc = pic_param->pic_fields.bits.weighted_bipred_idc,
    .pic_init_qp_minus26 = pic_param->pic_init_qp - 26,
    .pic_init_qs_minus26 = 0,
    .chroma_qp_index_offset = pic_param->chroma_qp_index_offset,
    .deblocking_filter_control_present_flag =
        pic_param->pic_fields.bits.deblocking_filter_control_present_flag,
    .constrained_intra_pred_flag =
        pic_param->pic_fields.bits.constrained_intra_pred_flag,
    .redundant_pic_cnt_present_flag =
        pic_param->pic_fields.bits.redundant_pic_cnt_present_flag,
    .transform_8x8_mode_flag =
        pic_param->pic_fields.bits.transform_8x8_mode_flag,
    /* unsupport scaling lists */
    .pic_scaling_matrix_present_flag = 0,
    .second_chroma_qp_index_offset = pic_param->second_chroma_qp_index_offset,
  };
  /* *INDENT-ON* */
}

static gboolean
_add_picture_header (GstVaH264Enc * self, GstVaH264EncFrame * frame,
    GstH264PPS * pps)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
#define PPS_SIZE 4 + GST_ROUND_UP_8 (MAX_PPS_HDR_SIZE) / 8
  guint8 packed_pps[PPS_SIZE] = { 0, };
#undef PPS_SIZE
  guint size;

  size = sizeof (packed_pps);
  if (gst_h264_bit_writer_pps (pps, TRUE, packed_pps,
          &size) != GST_H264_BIT_WRITER_OK) {
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
_add_one_slice (GstVaH264Enc * self, GstVaH264EncFrame * frame,
    gint start_mb, gint mb_size,
    VAEncSliceParameterBufferH264 * slice,
    GstVaH264EncFrame * list0[16], guint list0_num,
    GstVaH264EncFrame * list1[16], guint list1_num)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  int8_t slice_qp_delta = 0;
  gint i;

  /* *INDENT-OFF* */
  if (self->rc.rc_ctrl_mode == VA_RC_CQP) {
    if (frame->type == GST_H264_P_SLICE) {
      slice_qp_delta = self->rc.qp_p - self->rc.qp_i;
    } else if (frame->type == GST_H264_B_SLICE) {
      slice_qp_delta = (int8_t) (self->rc.qp_b - self->rc.qp_i);
    }
    g_assert (slice_qp_delta <= 51 && slice_qp_delta >= -51);
  }

  *slice = (VAEncSliceParameterBufferH264) {
    .macroblock_address = start_mb,
    .num_macroblocks = mb_size,
    .macroblock_info = VA_INVALID_ID,
    .slice_type = (uint8_t) frame->type,
    /* Only one parameter set supported now. */
    .pic_parameter_set_id = 0,
    .idr_pic_id = self->gop.total_idr_count,
    .pic_order_cnt_lsb = frame->poc,
    /* Not support top/bottom. */
    .delta_pic_order_cnt_bottom = 0,
    .delta_pic_order_cnt[0] = 0,
    .delta_pic_order_cnt[1] = 0,

    .direct_spatial_mv_pred_flag = TRUE,
    /* .num_ref_idx_active_override_flag = , */
    /* .num_ref_idx_l0_active_minus1 = , */
    /* .num_ref_idx_l1_active_minus1 = , */
    /* Set the reference list later. */

    .luma_log2_weight_denom = 0,
    .chroma_log2_weight_denom = 0,
    .luma_weight_l0_flag = 0,
    .chroma_weight_l0_flag = 0,
    .luma_weight_l1_flag = 0,
    .chroma_weight_l1_flag = 0,

    .cabac_init_idc = 0,
    /* Just use picture default setting. */
    .slice_qp_delta = slice_qp_delta,

    .disable_deblocking_filter_idc = 0,
    .slice_alpha_c0_offset_div2 = 2,
    .slice_beta_offset_div2 = 2,
  };
  /* *INDENT-ON* */

  if (frame->type == GST_H264_B_SLICE || frame->type == GST_H264_P_SLICE) {
    slice->num_ref_idx_active_override_flag = (list0_num > 0 || list1_num > 0);
    slice->num_ref_idx_l0_active_minus1 = list0_num > 0 ? list0_num - 1 : 0;
    if (frame->type == GST_H264_B_SLICE)
      slice->num_ref_idx_l1_active_minus1 = list1_num > 0 ? list1_num - 1 : 0;
  }

  i = 0;
  if (frame->type != GST_H264_I_SLICE) {
    for (; i < list0_num; i++) {
      slice->RefPicList0[i].picture_id =
          gst_va_encode_picture_get_reconstruct_surface (list0[i]->picture);
      slice->RefPicList0[i].TopFieldOrderCnt = list0[i]->poc;
      slice->RefPicList0[i].flags |= VA_PICTURE_H264_SHORT_TERM_REFERENCE;
      slice->RefPicList0[i].frame_idx = list0[i]->frame_num;
    }
  }
  for (; i < G_N_ELEMENTS (slice->RefPicList0); ++i) {
    slice->RefPicList0[i].picture_id = VA_INVALID_SURFACE;
    slice->RefPicList0[i].flags = VA_PICTURE_H264_INVALID;
  }

  i = 0;
  if (frame->type == GST_H264_B_SLICE) {
    for (; i < list1_num; i++) {
      slice->RefPicList1[i].picture_id =
          gst_va_encode_picture_get_reconstruct_surface (list1[i]->picture);
      slice->RefPicList1[i].TopFieldOrderCnt = list1[i]->poc;
      slice->RefPicList1[i].flags |= VA_PICTURE_H264_SHORT_TERM_REFERENCE;
      slice->RefPicList1[i].frame_idx = list1[i]->frame_num;
    }
  }
  for (; i < G_N_ELEMENTS (slice->RefPicList1); ++i) {
    slice->RefPicList1[i].picture_id = VA_INVALID_SURFACE;
    slice->RefPicList1[i].flags = VA_PICTURE_H264_INVALID;
  }

  if (!gst_va_encoder_add_param (base->encoder, frame->picture,
          VAEncSliceParameterBufferType, slice,
          sizeof (VAEncSliceParameterBufferH264))) {
    GST_ERROR_OBJECT (self, "Failed to create the slice parameter");
    return FALSE;
  }

  return TRUE;
}

static gint
_poc_asc_compare (const GstVaH264EncFrame ** a, const GstVaH264EncFrame ** b)
{
  return (*a)->poc - (*b)->poc;
}

static gint
_poc_des_compare (const GstVaH264EncFrame ** a, const GstVaH264EncFrame ** b)
{
  return (*b)->poc - (*a)->poc;
}

static gint
_frame_num_asc_compare (const GstVaH264EncFrame ** a,
    const GstVaH264EncFrame ** b)
{
  return (*a)->frame_num - (*b)->frame_num;
}

static gint
_frame_num_des_compare (const GstVaH264EncFrame ** a,
    const GstVaH264EncFrame ** b)
{
  return (*b)->frame_num - (*a)->frame_num;
}

/* If all the pic_num in the same order, OK. */
static gboolean
_ref_list_need_reorder (GstVaH264EncFrame * list[16], guint list_num,
    gboolean is_asc)
{
  guint i;
  gint pic_num_diff;

  if (list_num <= 1)
    return FALSE;

  for (i = 1; i < list_num; i++) {
    pic_num_diff = list[i]->frame_num - list[i - 1]->frame_num;
    g_assert (pic_num_diff != 0);

    if (pic_num_diff > 0 && !is_asc)
      return TRUE;

    if (pic_num_diff < 0 && is_asc)
      return TRUE;
  }

  return FALSE;
}

static void
_insert_ref_pic_list_modification (GstH264SliceHdr * slice_hdr,
    GstVaH264EncFrame * list[16], guint list_num, gboolean is_asc)
{
  GstVaH264EncFrame *list_by_pic_num[16] = { NULL, };
  guint modification_num, i;
  GstH264RefPicListModification *ref_pic_list_modification = NULL;
  gint pic_num_diff, pic_num_lx_pred;

  memcpy (list_by_pic_num, list, sizeof (GstVaH264EncFrame *) * list_num);

  if (is_asc) {
    g_qsort_with_data (list_by_pic_num, list_num, sizeof (gpointer),
        (GCompareDataFunc) _frame_num_asc_compare, NULL);
  } else {
    g_qsort_with_data (list_by_pic_num, list_num, sizeof (gpointer),
        (GCompareDataFunc) _frame_num_des_compare, NULL);
  }

  modification_num = 0;
  for (i = 0; i < list_num; i++) {
    if (list_by_pic_num[i]->poc != list[i]->poc)
      modification_num = i + 1;
  }
  g_assert (modification_num > 0);

  if (is_asc) {
    slice_hdr->ref_pic_list_modification_flag_l1 = 1;
    slice_hdr->n_ref_pic_list_modification_l1 =
        modification_num + 1 /* The end operation. */ ;
    ref_pic_list_modification = slice_hdr->ref_pic_list_modification_l1;
  } else {
    slice_hdr->ref_pic_list_modification_flag_l0 = 1;
    slice_hdr->n_ref_pic_list_modification_l0 =
        modification_num + 1 /* The end operation. */ ;
    ref_pic_list_modification = slice_hdr->ref_pic_list_modification_l0;
  }

  pic_num_lx_pred = slice_hdr->frame_num;
  for (i = 0; i < modification_num; i++) {
    pic_num_diff = list[i]->frame_num - pic_num_lx_pred;
    /* For the nex loop. */
    pic_num_lx_pred = list[i]->frame_num;

    g_assert (pic_num_diff != 0);

    if (pic_num_diff > 0) {
      ref_pic_list_modification->modification_of_pic_nums_idc = 1;
      ref_pic_list_modification->value.abs_diff_pic_num_minus1 =
          pic_num_diff - 1;
    } else {
      ref_pic_list_modification->modification_of_pic_nums_idc = 0;
      ref_pic_list_modification->value.abs_diff_pic_num_minus1 =
          (-pic_num_diff) - 1;
    }

    ref_pic_list_modification++;
  }

  ref_pic_list_modification->modification_of_pic_nums_idc = 3;
}

static void
_insert_ref_pic_marking_for_unused_frame (GstH264SliceHdr * slice_hdr,
    gint cur_frame_num, gint unused_frame_num)
{
  GstH264RefPicMarking *refpicmarking;

  slice_hdr->dec_ref_pic_marking.adaptive_ref_pic_marking_mode_flag = 1;
  slice_hdr->dec_ref_pic_marking.n_ref_pic_marking = 2;

  refpicmarking = &slice_hdr->dec_ref_pic_marking.ref_pic_marking[0];

  refpicmarking->memory_management_control_operation = 1;
  refpicmarking->difference_of_pic_nums_minus1 =
      cur_frame_num - unused_frame_num - 1;

  refpicmarking = &slice_hdr->dec_ref_pic_marking.ref_pic_marking[1];
  refpicmarking->memory_management_control_operation = 0;
}

static gboolean
_add_slice_header (GstVaH264Enc * self, GstVaH264EncFrame * frame,
    GstH264PPS * pps, VAEncSliceParameterBufferH264 * slice,
    GstVaH264EncFrame * list0[16], guint list0_num,
    GstVaH264EncFrame * list1[16], guint list1_num)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  GstH264SliceHdr slice_hdr;
  guint size, trail_bits;
  GstH264NalUnitType nal_type = GST_H264_NAL_SLICE;
#define SLICE_HDR_SIZE 4 + GST_ROUND_UP_8 (MAX_SLICE_HDR_SIZE) / 8
  guint8 packed_slice_hdr[SLICE_HDR_SIZE] = { 0, };
#undef SLICE_HDR_SIZE

  if (frame->frame_num == 0)
    nal_type = GST_H264_NAL_SLICE_IDR;

  /* *INDENT-OFF* */
  slice_hdr = (GstH264SliceHdr) {
    .first_mb_in_slice = slice->macroblock_address,
    .type = slice->slice_type,
    .pps = pps,
    .frame_num = frame->frame_num,
    /* interlaced not supported now. */
    .field_pic_flag = 0,
    .bottom_field_flag = 0,
    .idr_pic_id = (frame->frame_num == 0 ? slice->idr_pic_id : 0),
    /* only pic_order_cnt_type 1 is supported now. */
    .pic_order_cnt_lsb = slice->pic_order_cnt_lsb,
    .delta_pic_order_cnt_bottom = slice->delta_pic_order_cnt_bottom,
     /* Only for B frame. */
    .direct_spatial_mv_pred_flag =
        (frame->type == GST_H264_B_SLICE ?
         slice->direct_spatial_mv_pred_flag : 0),

    .num_ref_idx_active_override_flag = slice->num_ref_idx_active_override_flag,
    .num_ref_idx_l0_active_minus1 = slice->num_ref_idx_l0_active_minus1,
    .num_ref_idx_l1_active_minus1 = slice->num_ref_idx_l1_active_minus1,
    /* Calculate it later. */
    .ref_pic_list_modification_flag_l0 = 0,
    .ref_pic_list_modification_flag_l1 = 0,
    /* We have weighted_pred_flag and weighted_bipred_idc 0 here, no
     * need weight_table. */

    .dec_ref_pic_marking = {
      .no_output_of_prior_pics_flag = 0,
      .long_term_reference_flag = 0,
      /* If not sliding_window, we set it later. */
      .adaptive_ref_pic_marking_mode_flag = 0,
    },

    .cabac_init_idc = slice->cabac_init_idc,
    .slice_qp_delta = slice->slice_qp_delta,

    .disable_deblocking_filter_idc = slice->disable_deblocking_filter_idc,
    .slice_alpha_c0_offset_div2 = slice->slice_alpha_c0_offset_div2,
    .slice_beta_offset_div2 = slice->slice_beta_offset_div2,
  };
  /* *INDENT-ON* */

  /* Reorder the ref lists if needed. */
  if (list0_num > 1) {
    /* list0 is in poc descend order now. */
    if (_ref_list_need_reorder (list0, list0_num, FALSE))
      _insert_ref_pic_list_modification (&slice_hdr, list0, list0_num, FALSE);
  }

  if (list0_num > 1) {
    /* list0 is in poc ascend order now. */
    if (_ref_list_need_reorder (list1, list1_num, TRUE)) {
      _insert_ref_pic_list_modification (&slice_hdr, list1, list1_num, TRUE);
    }
  }

  /* Mark the unused reference explicitly which this frame replaces. */
  if (frame->unused_for_reference_pic_num >= 0) {
    g_assert (frame->is_ref);
    _insert_ref_pic_marking_for_unused_frame (&slice_hdr, frame->frame_num,
        frame->unused_for_reference_pic_num);
  }

  size = sizeof (packed_slice_hdr);
  trail_bits = 0;
  if (gst_h264_bit_writer_slice_hdr (&slice_hdr, TRUE, nal_type, frame->is_ref,
          packed_slice_hdr, &size, &trail_bits) != GST_H264_BIT_WRITER_OK) {
    GST_ERROR_OBJECT (self, "Failed to generate the slice header");
    return FALSE;
  }

  if (!gst_va_encoder_add_packed_header (base->encoder, frame->picture,
          VAEncPackedHeaderSlice, packed_slice_hdr, size * 8 + trail_bits,
          FALSE)) {
    GST_ERROR_OBJECT (self, "Failed to add the packed slice header");
    return FALSE;
  }

  return TRUE;
}

static gboolean
_add_aud (GstVaH264Enc * self, GstVaH264EncFrame * frame)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  guint8 aud_data[8] = { 0, };
  guint size;
  guint8 primary_pic_type = 0;

  switch (frame->type) {
    case GST_H264_I_SLICE:
      primary_pic_type = 0;
      break;
    case GST_H264_P_SLICE:
      primary_pic_type = 1;
      break;
    case GST_H264_B_SLICE:
      primary_pic_type = 2;
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  size = sizeof (aud_data);
  if (gst_h264_bit_writer_aud (primary_pic_type, TRUE, aud_data,
          &size) != GST_H264_BIT_WRITER_OK) {
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

static void
_create_sei_cc_message (GstVideoCaptionMeta * cc_meta,
    GstH264SEIMessage * sei_msg)
{
  guint8 *data;
  GstH264RegisteredUserData *user_data;

  sei_msg->payloadType = GST_H264_SEI_REGISTERED_USER_DATA;

  user_data = &sei_msg->payload.registered_user_data;

  user_data->country_code = 181;
  user_data->size = 10 + cc_meta->size;

  data = g_malloc (user_data->size);

  /* 16-bits itu_t_t35_provider_code */
  data[0] = 0;
  data[1] = 49;
  /* 32-bits ATSC_user_identifier */
  data[2] = 'G';
  data[3] = 'A';
  data[4] = '9';
  data[5] = '4';
  /* 8-bits ATSC1_data_user_data_type_code */
  data[6] = 3;
  /* 8-bits:
   * 1 bit process_em_data_flag (0)
   * 1 bit process_cc_data_flag (1)
   * 1 bit additional_data_flag (0)
   * 5-bits cc_count
   */
  data[7] = ((cc_meta->size / 3) & 0x1f) | 0x40;
  /* 8 bits em_data, unused */
  data[8] = 255;

  memcpy (data + 9, cc_meta->data, cc_meta->size);

  /* 8 marker bits */
  data[user_data->size - 1] = 255;

  user_data->data = data;
}

static gboolean
_create_sei_cc_data (GPtrArray * cc_list, guint8 * sei_data, guint * data_size)
{
  GArray *msg_list = NULL;
  GstH264BitWriterResult ret;
  gint i;

  msg_list = g_array_new (TRUE, TRUE, sizeof (GstH264SEIMessage));
  g_array_set_clear_func (msg_list, (GDestroyNotify) gst_h264_sei_clear);
  g_array_set_size (msg_list, cc_list->len);

  for (i = 0; i < cc_list->len; i++) {
    GstH264SEIMessage *msg = &g_array_index (msg_list, GstH264SEIMessage, i);
    _create_sei_cc_message (g_ptr_array_index (cc_list, i), msg);
  }

  ret = gst_h264_bit_writer_sei (msg_list, TRUE, sei_data, data_size);

  g_array_unref (msg_list);

  return (ret == GST_H264_BIT_WRITER_OK);
}

static void
_add_sei_cc (GstVaH264Enc * self, GstVideoCodecFrame * gst_frame)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  GstVaH264EncFrame *frame;
  GPtrArray *cc_list = NULL;
  GstVideoCaptionMeta *cc_meta;
  gpointer iter = NULL;
  guint8 *packed_sei = NULL;
  guint sei_size = 0;

  frame = _enc_frame (gst_frame);

  /* SEI header size */
  sei_size = 6;
  while ((cc_meta = (GstVideoCaptionMeta *)
          gst_buffer_iterate_meta_filtered (gst_frame->input_buffer, &iter,
              GST_VIDEO_CAPTION_META_API_TYPE))) {
    if (cc_meta->caption_type != GST_VIDEO_CAPTION_TYPE_CEA708_RAW)
      continue;

    if (!cc_list)
      cc_list = g_ptr_array_new ();

    g_ptr_array_add (cc_list, cc_meta);
    /* Add enough SEI message size for bitwriter. */
    sei_size += cc_meta->size + 50;
  }

  if (!cc_list)
    goto out;

  packed_sei = g_malloc0 (sei_size);

  if (!_create_sei_cc_data (cc_list, packed_sei, &sei_size)) {
    GST_WARNING_OBJECT (self, "Failed to write the SEI CC data");
    goto out;
  }

  if (!gst_va_encoder_add_packed_header (base->encoder, frame->picture,
          VAEncPackedHeaderRawData, packed_sei, sei_size * 8, FALSE)) {
    GST_WARNING_OBJECT (self, "Failed to add SEI CC data");
    goto out;
  }

out:
  g_clear_pointer (&cc_list, g_ptr_array_unref);
  if (packed_sei)
    g_free (packed_sei);
}

static gboolean
_encode_one_frame (GstVaH264Enc * self, GstVideoCodecFrame * gst_frame)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  VAEncPictureParameterBufferH264 pic_param;
  GstH264PPS pps;
  GstVaH264EncFrame *list0[16] = { NULL, };
  guint list0_num = 0;
  GstVaH264EncFrame *list1[16] = { NULL, };
  guint list1_num = 0;
  guint slice_of_mbs, slice_mod_mbs, slice_start_mb, slice_mbs;
  gint i;
  GstVaH264EncFrame *frame;

  g_return_val_if_fail (gst_frame, FALSE);

  frame = _enc_frame (gst_frame);

  if (self->aud && !_add_aud (self, frame))
    return FALSE;

  /* Repeat the SPS for IDR. */
  if (frame->poc == 0) {
    VAEncSequenceParameterBufferH264 sequence;

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
            self->use_trellis))
      return FALSE;

    _fill_sequence_param (self, &sequence);
    if (!_fill_sps (self, &sequence))
      return FALSE;

    if (!_add_sequence_parameter (self, frame->picture, &sequence))
      return FALSE;

    if ((self->packed_headers & VA_ENC_PACKED_HEADER_SEQUENCE)
        && !_add_sequence_header (self, frame))
      return FALSE;
  }

  /* Non I frame, construct reference list. */
  if (frame->type != GST_H264_I_SLICE) {
    GstVaH264EncFrame *vaf;
    GstVideoCodecFrame *f;

    for (i = g_queue_get_length (&base->ref_list) - 1; i >= 0; i--) {
      f = g_queue_peek_nth (&base->ref_list, i);
      vaf = _enc_frame (f);
      if (vaf->poc > frame->poc)
        continue;

      list0[list0_num] = vaf;
      list0_num++;
    }

    /* reorder to select the most nearest forward frames. */
    g_qsort_with_data (list0, list0_num, sizeof (gpointer),
        (GCompareDataFunc) _poc_des_compare, NULL);

    if (list0_num > self->gop.ref_num_list0)
      list0_num = self->gop.ref_num_list0;
  }

  if (frame->type == GST_H264_B_SLICE) {
    GstVaH264EncFrame *vaf;
    GstVideoCodecFrame *f;

    for (i = 0; i < g_queue_get_length (&base->ref_list); i++) {
      f = g_queue_peek_nth (&base->ref_list, i);
      vaf = _enc_frame (f);
      if (vaf->poc < frame->poc)
        continue;

      list1[list1_num] = vaf;
      list1_num++;
    }

    /* reorder to select the most nearest backward frames. */
    g_qsort_with_data (list1, list1_num, sizeof (gpointer),
        (GCompareDataFunc) _poc_asc_compare, NULL);

    if (list1_num > self->gop.ref_num_list1)
      list1_num = self->gop.ref_num_list1;
  }

  g_assert (list0_num + list1_num <= self->gop.num_ref_frames);

  if (!_fill_picture_parameter (self, frame, &pic_param))
    return FALSE;
  if (!_add_picture_parameter (self, frame, &pic_param))
    return FALSE;
  _fill_pps (&pic_param, &self->sequence_hdr, &pps);

  if ((self->packed_headers & VA_ENC_PACKED_HEADER_PICTURE)
      && frame->type == GST_H264_I_SLICE
      && !_add_picture_header (self, frame, &pps))
    return FALSE;

  if (self->cc) {
    /* CC errors are not fatal */
    _add_sei_cc (self, gst_frame);
  }

  slice_of_mbs = self->mb_width * self->mb_height / self->num_slices;
  slice_mod_mbs = self->mb_width * self->mb_height % self->num_slices;
  slice_start_mb = 0;
  slice_mbs = 0;
  for (i = 0; i < self->num_slices; i++) {
    VAEncSliceParameterBufferH264 slice;

    slice_mbs = slice_of_mbs;
    /* divide the remainder to each equally */
    if (slice_mod_mbs) {
      slice_mbs++;
      slice_mod_mbs--;
    }

    if (!_add_one_slice (self, frame, slice_start_mb, slice_mbs, &slice,
            list0, list0_num, list1, list1_num))
      return FALSE;

    if ((self->packed_headers & VA_ENC_PACKED_HEADER_SLICE) &&
        (!_add_slice_header (self, frame, &pps, &slice, list0, list0_num,
                list1, list1_num)))
      return FALSE;

    slice_start_mb += slice_mbs;
  }

  if (!gst_va_encoder_encode (base->encoder, frame->picture)) {
    GST_ERROR_OBJECT (self, "Encode frame error");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_va_h264_enc_flush (GstVideoEncoder * venc)
{
  GstVaH264Enc *self = GST_VA_H264_ENC (venc);

  /* begin from an IDR after flush. */
  self->gop.cur_frame_index = 0;
  self->gop.cur_frame_num = 0;
  self->gop.last_keyframe = NULL;

  return GST_VIDEO_ENCODER_CLASS (parent_class)->flush (venc);
}

static gboolean
gst_va_h264_enc_prepare_output (GstVaBaseEnc * base,
    GstVideoCodecFrame * frame, gboolean * complete)
{
  GstVaH264EncFrame *frame_enc;
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

static gint
_sort_by_frame_num (gconstpointer a, gconstpointer b, gpointer user_data)
{
  GstVaH264EncFrame *frame1 = _enc_frame ((GstVideoCodecFrame *) a);
  GstVaH264EncFrame *frame2 = _enc_frame ((GstVideoCodecFrame *) b);

  g_assert (frame1->frame_num != frame2->frame_num);

  return frame1->frame_num - frame2->frame_num;
}

static GstVideoCodecFrame *
_find_unused_reference_frame (GstVaH264Enc * self, GstVaH264EncFrame * frame)
{
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);
  GstVaH264EncFrame *b_vaframe;
  GstVideoCodecFrame *b_frame;
  guint i;

  /* We still have more space. */
  if (g_queue_get_length (&base->ref_list) < self->gop.num_ref_frames)
    return NULL;

  /* Not b_pyramid, sliding window is enough. */
  if (!self->gop.b_pyramid)
    return g_queue_peek_head (&base->ref_list);

  /* I/P frame, just using sliding window. */
  if (frame->type != GST_H264_B_SLICE)
    return g_queue_peek_head (&base->ref_list);

  /* Choose the B frame with lowest POC. */
  b_frame = NULL;
  b_vaframe = NULL;
  for (i = 0; i < g_queue_get_length (&base->ref_list); i++) {
    GstVaH264EncFrame *vaf;
    GstVideoCodecFrame *f;

    f = g_queue_peek_nth (&base->ref_list, i);
    vaf = _enc_frame (f);
    if (vaf->type != GST_H264_B_SLICE)
      continue;

    if (!b_frame) {
      b_frame = f;
      b_vaframe = _enc_frame (b_frame);
      continue;
    }

    b_vaframe = _enc_frame (b_frame);
    g_assert (vaf->poc != b_vaframe->poc);
    if (vaf->poc < b_vaframe->poc) {
      b_frame = f;
      b_vaframe = _enc_frame (b_frame);
    }
  }

  /* No B frame as ref. */
  if (!b_frame)
    return g_queue_peek_head (&base->ref_list);

  if (b_frame != g_queue_peek_head (&base->ref_list)) {
    b_vaframe = _enc_frame (b_frame);
    frame->unused_for_reference_pic_num = b_vaframe->frame_num;
    GST_LOG_OBJECT (self, "The frame with POC: %d, pic_num %d will be"
        " replaced by the frame with POC: %d, pic_num %d explicitly by"
        " using memory_management_control_operation=1",
        b_vaframe->poc, b_vaframe->frame_num, frame->poc, frame->frame_num);
  }

  return b_frame;
}

static GstFlowReturn
gst_va_h264_enc_encode_frame (GstVaBaseEnc * base,
    GstVideoCodecFrame * gst_frame, gboolean is_last)
{
  GstVaH264Enc *self = GST_VA_H264_ENC (base);
  GstVaH264EncFrame *frame;
  GstVideoCodecFrame *unused_ref = NULL;

  frame = _enc_frame (gst_frame);
  frame->last_frame = is_last;

  g_assert (frame->picture == NULL);
  frame->picture = gst_va_encode_picture_new (base->encoder,
      gst_frame->input_buffer);

  if (!frame->picture) {
    GST_ERROR_OBJECT (self, "Failed to create the encode picture");
    return GST_FLOW_ERROR;
  }

  if (frame->is_ref)
    unused_ref = _find_unused_reference_frame (self, frame);

  if (!_encode_one_frame (self, gst_frame)) {
    GST_ERROR_OBJECT (self, "Failed to encode the frame");
    return GST_FLOW_ERROR;
  }

  g_queue_push_tail (&base->output_list, gst_video_codec_frame_ref (gst_frame));

  if (frame->is_ref) {
    if (unused_ref) {
      if (!g_queue_remove (&base->ref_list, unused_ref))
        g_assert_not_reached ();

      gst_video_codec_frame_unref (unused_ref);
    }

    /* Add it into the reference list. */
    g_queue_push_tail (&base->ref_list, gst_video_codec_frame_ref (gst_frame));
    g_queue_sort (&base->ref_list, _sort_by_frame_num, NULL);

    g_assert (g_queue_get_length (&base->ref_list) <= self->gop.num_ref_frames);
  }

  return GST_FLOW_OK;
}

static gboolean
gst_va_h264_enc_new_frame (GstVaBaseEnc * base, GstVideoCodecFrame * frame)
{
  GstVaH264Enc *self = GST_VA_H264_ENC (base);
  GstVaH264EncFrame *frame_in;

  frame_in = gst_va_enc_frame_new ();
  gst_video_codec_frame_set_user_data (frame, frame_in, gst_va_enc_frame_free);

  gst_va_base_enc_push_dts (base, frame, self->gop.num_reorder_frames);

  return TRUE;
}

static gboolean
gst_va_h264_enc_start (GstVideoEncoder * venc)
{
  /* Set the minimum pts to some huge value (1000 hours). This keeps
   * the dts at the start of the stream from needing to be negative. */
  gst_video_encoder_set_min_pts (venc, GST_SECOND * 60 * 60 * 1000);

  return GST_VIDEO_ENCODER_CLASS (parent_class)->start (venc);
}

/* *INDENT-OFF* */
static const gchar *sink_caps_str =
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_VA,
        "{ NV12 }") " ;"
    GST_VIDEO_CAPS_MAKE ("{ NV12 }");
/* *INDENT-ON* */

static const gchar *src_caps_str = "video/x-h264";

static gpointer
_register_debug_category (gpointer data)
{
  GST_DEBUG_CATEGORY_INIT (gst_va_h264enc_debug, "vah264enc", 0,
      "VA h264 encoder");

  return NULL;
}

static void
gst_va_h264_enc_init (GTypeInstance * instance, gpointer g_class)
{
  GstVaH264Enc *self = GST_VA_H264_ENC (instance);

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
  self->prop.use_dct8x8 = TRUE;
  self->prop.use_cabac = TRUE;
  self->prop.use_trellis = FALSE;
  self->prop.aud = FALSE;
  self->prop.cc = TRUE;
  self->prop.mbbrc = 0;
  self->prop.bitrate = 0;
  self->prop.target_percentage = 66;
  self->prop.target_usage = 4;
  if (properties[PROP_RATE_CONTROL]) {
    self->prop.rc_ctrl =
        G_PARAM_SPEC_ENUM (properties[PROP_RATE_CONTROL])->default_value;
  } else {
    self->prop.rc_ctrl = VA_RC_NONE;
  }
  self->prop.cpb_size = 0;
}

static void
gst_va_h264_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVaH264Enc *self = GST_VA_H264_ENC (object);
  GstVaBaseEnc *base = GST_VA_BASE_ENC (self);

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
      break;
    case PROP_QP_P:
      self->prop.qp_p = g_value_get_uint (value);
      g_atomic_int_set (&GST_VA_BASE_ENC (self)->reconf, TRUE);
      break;
    case PROP_QP_B:
      self->prop.qp_b = g_value_get_uint (value);
      g_atomic_int_set (&GST_VA_BASE_ENC (self)->reconf, TRUE);
      break;
    case PROP_DCT8X8:
      self->prop.use_dct8x8 = g_value_get_boolean (value);
      break;
    case PROP_CABAC:
      self->prop.use_cabac = g_value_get_boolean (value);
      break;
    case PROP_TRELLIS:
      self->prop.use_trellis = g_value_get_boolean (value);
      break;
    case PROP_AUD:
      self->prop.aud = g_value_get_boolean (value);
      break;
    case PROP_CC:
      self->prop.cc = g_value_get_boolean (value);
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
      break;
    case PROP_TARGET_PERCENTAGE:
      self->prop.target_percentage = g_value_get_uint (value);
      g_atomic_int_set (&GST_VA_BASE_ENC (self)->reconf, TRUE);
      break;
    case PROP_TARGET_USAGE:
      self->prop.target_usage = g_value_get_uint (value);
      g_atomic_int_set (&GST_VA_BASE_ENC (self)->reconf, TRUE);
      break;
    case PROP_RATE_CONTROL:
      self->prop.rc_ctrl = g_value_get_enum (value);
      g_atomic_int_set (&GST_VA_BASE_ENC (self)->reconf, TRUE);
      break;
    case PROP_CPB_SIZE:
      self->prop.cpb_size = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }

  GST_OBJECT_UNLOCK (self);

#ifndef GST_DISABLE_GST_DEBUG
  if (!g_atomic_int_get (&GST_VA_BASE_ENC (self)->reconf)
      && base->encoder && gst_va_encoder_is_open (base->encoder)) {
    GST_WARNING_OBJECT (self, "Property `%s` change ignored while processing.",
        pspec->name);
  }
#endif

}

static void
gst_va_h264_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVaH264Enc *const self = GST_VA_H264_ENC (object);

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
    case PROP_DCT8X8:
      g_value_set_boolean (value, self->prop.use_dct8x8);
      break;
    case PROP_CABAC:
      g_value_set_boolean (value, self->prop.use_cabac);
      break;
    case PROP_TRELLIS:
      g_value_set_boolean (value, self->prop.use_trellis);
      break;
    case PROP_AUD:
      g_value_set_boolean (value, self->prop.aud);
      break;
    case PROP_CC:
      g_value_set_boolean (value, self->prop.cc);
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
gst_va_h264_enc_class_init (gpointer g_klass, gpointer class_data)
{
  GstCaps *src_doc_caps, *sink_doc_caps;
  GstPadTemplate *sink_pad_templ, *src_pad_templ;
  GObjectClass *object_class = G_OBJECT_CLASS (g_klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_klass);
  GstVideoEncoderClass *venc_class = GST_VIDEO_ENCODER_CLASS (g_klass);
  GstVaBaseEncClass *va_enc_class = GST_VA_BASE_ENC_CLASS (g_klass);
  GstVaH264EncClass *vah264enc_class = GST_VA_H264_ENC_CLASS (g_klass);
  GstVaDisplay *display;
  GstVaEncoder *encoder;
  struct CData *cdata = class_data;
  gchar *long_name;
  const gchar *name, *desc;
  gint n_props = N_PROPERTIES;
  GParamFlags param_flags =
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT;

  if (cdata->entrypoint == VAEntrypointEncSlice) {
    desc = "VA-API based H.264 video encoder";
    name = "VA-API H.264 Encoder";
  } else {
    desc = "VA-API based H.264 low power video encoder";
    name = "VA-API H.264 Low Power Encoder";
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

  va_enc_class->codec = H264;
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

  object_class->set_property = gst_va_h264_enc_set_property;
  object_class->get_property = gst_va_h264_enc_get_property;

  venc_class->flush = GST_DEBUG_FUNCPTR (gst_va_h264_enc_flush);
  venc_class->start = GST_DEBUG_FUNCPTR (gst_va_h264_enc_start);

  va_enc_class->reset_state = GST_DEBUG_FUNCPTR (gst_va_h264_enc_reset_state);
  va_enc_class->reconfig = GST_DEBUG_FUNCPTR (gst_va_h264_enc_reconfig);
  va_enc_class->new_frame = GST_DEBUG_FUNCPTR (gst_va_h264_enc_new_frame);
  va_enc_class->reorder_frame =
      GST_DEBUG_FUNCPTR (gst_va_h264_enc_reorder_frame);
  va_enc_class->encode_frame = GST_DEBUG_FUNCPTR (gst_va_h264_enc_encode_frame);
  va_enc_class->prepare_output =
      GST_DEBUG_FUNCPTR (gst_va_h264_enc_prepare_output);

  {
    display = gst_va_display_platform_new (va_enc_class->render_device_path);
    encoder = gst_va_encoder_new (display, va_enc_class->codec,
        va_enc_class->entrypoint);
    if (gst_va_encoder_get_rate_control_enum (encoder,
            vah264enc_class->rate_control)) {
      gchar *basename = g_path_get_basename (va_enc_class->render_device_path);
      g_snprintf (vah264enc_class->rate_control_type_name,
          G_N_ELEMENTS (vah264enc_class->rate_control_type_name) - 1,
          "GstVaEncoderRateControl_%" GST_FOURCC_FORMAT "%s_%s",
          GST_FOURCC_ARGS (va_enc_class->codec),
          (va_enc_class->entrypoint == VAEntrypointEncSliceLP) ? "_LP" : "",
          basename);
      vah264enc_class->rate_control_type =
          g_enum_register_static (vah264enc_class->rate_control_type_name,
          vah264enc_class->rate_control);
      gst_type_mark_as_plugin_api (vah264enc_class->rate_control_type, 0);
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
   * GstVaH264Enc:key-int-max:
   *
   * The maximal distance between two keyframes.
   */
  properties[PROP_KEY_INT_MAX] = g_param_spec_uint ("key-int-max",
      "Key frame maximal interval",
      "The maximal distance between two keyframes. It decides the size of GOP"
      " (0: auto-calculate)", 0, MAX_GOP_SIZE, 0, param_flags);

  /**
   * GstVaH264Enc:b-frames:
   *
   * Number of B-frames between two reference frames.
   */
  properties[PROP_BFRAMES] = g_param_spec_uint ("b-frames", "B Frames",
      "Number of B frames between I and P reference frames", 0, 31, 0,
      param_flags);

  /**
   * GstVaH264Enc:i-frames:
   *
   * Force the number of i-frames insertion within one GOP.
   */
  properties[PROP_IFRAMES] = g_param_spec_uint ("i-frames", "I Frames",
      "Force the number of I frames insertion within one GOP, not including the "
      "first IDR frame", 0, 1023, 0, param_flags);

  /**
   * GstVaH264Enc:ref-frames:
   *
   * The number of reference frames.
   */
  properties[PROP_NUM_REF_FRAMES] = g_param_spec_uint ("ref-frames",
      "Number of Reference Frames",
      "Number of reference frames, including both the forward and the backward",
      0, 16, 3, param_flags);

  /**
   * GstVaH264Enc:b-pyramid:
   *
   * Enable the b-pyramid reference structure in GOP.
   */
  properties[PROP_B_PYRAMID] = g_param_spec_boolean ("b-pyramid", "b pyramid",
      "Enable the b-pyramid reference structure in the GOP", FALSE,
      param_flags);

  /**
   * GstVaH264Enc:num-slices:
   *
   * The number of slices per frame.
   */
  properties[PROP_NUM_SLICES] = g_param_spec_uint ("num-slices",
      "Number of Slices", "Number of slices per frame", 1, 200, 1, param_flags);

  /**
   * GstVaH264Enc:max-qp:
   *
   * The maximum quantizer value.
   */
  properties[PROP_MAX_QP] = g_param_spec_uint ("max-qp", "Maximum QP",
      "Maximum quantizer value for each frame", 0, 51, 51, param_flags);

  /**
   * GstVaH264Enc:min-qp:
   *
   * The minimum quantizer value.
   */
  properties[PROP_MIN_QP] = g_param_spec_uint ("min-qp", "Minimum QP",
      "Minimum quantizer value for each frame", 0, 51, 1, param_flags);

  /**
   * GstVaH264Enc:qpi:
   *
   * The quantizer value for I frame.
   *
   * In CQP mode, it specifies the QP of I frame, in other mode, it specifies
   * the init QP of all frames.
   */
  properties[PROP_QP_I] = g_param_spec_uint ("qpi", "I Frame QP",
      "The quantizer value for I frame. In CQP mode, it specifies the QP of I "
      "frame, in other mode, it specifies the init QP of all frames", 0, 51, 26,
      param_flags | GST_PARAM_MUTABLE_PLAYING);

  /**
   * GstVaH264Enc:qpp:
   *
   * The quantizer value for P frame. Available only in CQP mode.
   */
  properties[PROP_QP_P] = g_param_spec_uint ("qpp",
      "The quantizer value for P frame",
      "The quantizer value for P frame. Available only in CQP mode",
      0, 51, 26, param_flags | GST_PARAM_MUTABLE_PLAYING);

  /**
   * GstVaH264Enc:qpb:
   *
   * The quantizer value for B frame. Available only in CQP mode.
   */
  properties[PROP_QP_B] = g_param_spec_uint ("qpb",
      "The quantizer value for B frame",
      "The quantizer value for B frame. Available only in CQP mode",
      0, 51, 26, param_flags | GST_PARAM_MUTABLE_PLAYING);

  /**
   * GstVaH264Enc:dct8x8:
   *
   * Enable adaptive use of 8x8 transforms in I-frames. This improves
   * the compression ratio but requires high profile at least.
   */
  properties[PROP_DCT8X8] = g_param_spec_boolean ("dct8x8", "Enable 8x8 DCT",
      "Enable adaptive use of 8x8 transforms in I-frames", TRUE, param_flags);

  /**
   * GstVaH264Enc:cabac:
   *
   * It enables CABAC entropy coding mode to improve compression ratio,
   * but requires main profile at least.
   */
  properties[PROP_CABAC] = g_param_spec_boolean ("cabac", "Enable CABAC",
      "Enable CABAC entropy coding mode", TRUE, param_flags);

  /**
   * GstVaH264Enc:trellis:
   *
   * It enable the trellis quantization method. Trellis is an improved
   * quantization algorithm.
   */
  properties[PROP_TRELLIS] = g_param_spec_boolean ("trellis", "Enable trellis",
      "Enable the trellis quantization method", FALSE, param_flags);

  /**
   * GstVaH264Enc:aud:
   *
   * Insert the AU (Access Unit) delimeter for each frame.
   */
  properties[PROP_AUD] = g_param_spec_boolean ("aud", "Insert AUD",
      "Insert AU (Access Unit) delimeter for each frame", FALSE, param_flags);

  /**
   * GstVaH264Enc:cc-insert:
   *
   * Closed Caption Insert mode. Only CEA-708 RAW format is supported for now.
   */
  properties[PROP_CC] = g_param_spec_boolean ("cc-insert",
      "Insert Closed Captions",
      "Insert CEA-708 Closed Captions",
      TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

  /**
   * GstVaH264Enc:mbbrc:
   *
   * Macroblock level bitrate control. Not available in CQP mode.
   */
  properties[PROP_MBBRC] = g_param_spec_enum ("mbbrc",
      "Macroblock level Bitrate Control",
      "Macroblock level Bitrate Control. Not available in CQP mode",
      GST_TYPE_VA_FEATURE, GST_VA_FEATURE_AUTO, param_flags);

  /**
   * GstVaH264Enc:bitrate:
   *
   * The desired target bitrate, expressed in kbps. Not available in CQP mode.
   *
   * * **CBR**: This applies equally to the minimum, maximum and target bitrate.
   * * **VBR**: This applies to the target bitrate. The driver will use the
   *   "target-percentage" together to calculate the minimum and maximum
   *   bitrate.
   * * **VCM**: This applies to the target bitrate. The minimum and maximum
   *   bitrate are not needed.
   */
  properties[PROP_BITRATE] = g_param_spec_uint ("bitrate", "Bitrate (kbps)",
      "The desired bitrate expressed in kbps (0: auto-calculate)",
      0, 2000 * 1024, 0, param_flags | GST_PARAM_MUTABLE_PLAYING);

  /**
   * GstVaH264Enc:target-percentage:
   *
   * The target percentage of the max bitrate, and expressed in uint, equal to
   * "target percentage" * 100. Available only when rate-control is VBR.
   *
   * "target percentage" = "target bitrate" * 100 /  "max bitrate"
   *
   * The driver uses it to calculate the minimum and maximum bitrate.
   */
  properties[PROP_TARGET_PERCENTAGE] = g_param_spec_uint ("target-percentage",
      "target bitrate percentage",
      "The percentage for 'target bitrate'/'maximum bitrate' (Only in VBR)",
      50, 100, 66, param_flags | GST_PARAM_MUTABLE_PLAYING);

  /**
   * GstVaH264Enc:target-usage:
   *
   * The target usage of the encoder.
   *
   * It controls and balances the encoding speed and the encoding quality. The
   * lower value has better quality but slower speed, the higher value has
   * faster speed but lower quality.
   */
  properties[PROP_TARGET_USAGE] = g_param_spec_uint ("target-usage",
      "target usage",
      "The target usage to control and balance the encoding speed/quality",
      1, 7, 4, param_flags | GST_PARAM_MUTABLE_PLAYING);

  /**
   * GstVaH264Enc:cpb-size:
   *
   * The desired max CPB size in Kb (0: auto-calculate).
   */
  properties[PROP_CPB_SIZE] = g_param_spec_uint ("cpb-size",
      "max CPB size in Kb",
      "The desired max CPB size in Kb (0: auto-calculate)", 0, 2000 * 1024, 0,
      param_flags | GST_PARAM_MUTABLE_PLAYING);

  if (vah264enc_class->rate_control_type > 0) {
    properties[PROP_RATE_CONTROL] = g_param_spec_enum ("rate-control",
        "rate control mode", "The desired rate control mode for the encoder",
        vah264enc_class->rate_control_type,
        vah264enc_class->rate_control[0].value,
        GST_PARAM_CONDITIONALLY_AVAILABLE | GST_PARAM_MUTABLE_PLAYING
        | param_flags);
  } else {
    n_props--;
    properties[PROP_RATE_CONTROL] = NULL;
  }

  g_object_class_install_properties (object_class, n_props, properties);
}

static GstCaps *
_complete_src_caps (GstCaps * srccaps)
{
  GstCaps *caps = gst_caps_copy (srccaps);

  gst_caps_set_simple (caps, "alignment", G_TYPE_STRING, "au", "stream-format",
      G_TYPE_STRING, "byte-stream", NULL);

  return caps;
}

gboolean
gst_va_h264_enc_register (GstPlugin * plugin, GstVaDevice * device,
    GstCaps * sink_caps, GstCaps * src_caps, guint rank,
    VAEntrypoint entrypoint)
{
  static GOnce debug_once = G_ONCE_INIT;
  GType type;
  GTypeInfo type_info = {
    .class_size = sizeof (GstVaH264EncClass),
    .class_init = gst_va_h264_enc_class_init,
    .instance_size = sizeof (GstVaH264Enc),
    .instance_init = gst_va_h264_enc_init,
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
    gst_va_create_feature_name (device, "GstVaH264Enc", "GstVa%sH264Enc",
        &type_name, "vah264enc", "va%sh264enc", &feature_name,
        &cdata->description, &rank);
  } else {
    gst_va_create_feature_name (device, "GstVaH264LPEnc", "GstVa%sH264LPEnc",
        &type_name, "vah264lpenc", "va%sh264lpenc", &feature_name,
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
