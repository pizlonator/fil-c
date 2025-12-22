/* GStreamer
 * Copyright (C) <2017> Sean DuBois <sean@siobud.com>
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


#ifndef __GST_AV1_ENC_H__
#define __GST_AV1_ENC_H__


#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideoencoder.h>

#include <aom/aom_codec.h>
#include <aom/aom_encoder.h>
#include <aom/aomcx.h>

G_BEGIN_DECLS
#define GST_TYPE_AV1_ENC \
  (gst_av1_enc_get_type())
#define GST_AV1_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AV1_ENC,GstAV1Enc))
#define GST_AV1_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AV1_ENC,GstAV1EncClass))
#define GST_IS_AV1_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AV1_ENC))
#define GST_IS_AV1_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AV1_ENC))
#define GST_AV1_ENC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_AV1_ENC, GstAV1EncClass))
#define GST_AV1_ENC_CAST(obj) \
  ((GstAV1Enc *) (obj))

typedef struct _GstAV1Enc GstAV1Enc;
typedef struct _GstAV1EncClass GstAV1EncClass;

/**
 * GstAV1EncResizeMode:
 * @GST_AV1_ENC_RESIZE_NONE: No frame resizing allowed
 * @GST_AV1_ENC_RESIZE_FIXED: All frames are coded at the specified scale
 * @GST_AV1_ENC_RESIZE_RANDOM: All frames are coded at a random scale
 *
 * Frame resize mode
 */
typedef enum
{
  GST_AV1_ENC_RESIZE_NONE = 0,
  GST_AV1_ENC_RESIZE_FIXED = 1,
  GST_AV1_ENC_RESIZE_RANDOM = 2,
} GstAV1EncResizeMode;

/**
 * GstAV1EncSuperresMode:
 * @GST_AV1_ENC_SUPERRES_NONE: No frame superres allowed
 * @GST_AV1_ENC_SUPERRES_FIXED: All frames are coded at the specified scale and
 *   super-resolved
 * @GST_AV1_ENC_SUPERRES_QTHRESH: Superres scale for a frame is determined based
 *   on q_index
 *
 * Frame super-resolution mode
 */
typedef enum
{
  GST_AV1_ENC_SUPERRES_NONE = 0,
  GST_AV1_ENC_SUPERRES_FIXED = 1,
  GST_AV1_ENC_SUPERRES_RANDOM = 2,
  GST_AV1_ENC_SUPERRES_QTHRESH = 3,
} GstAV1EncSuperresMode;

#ifdef HAVE_LIBAOM_3
G_STATIC_ASSERT ((guint) GST_AV1_ENC_SUPERRES_NONE == (guint) AOM_SUPERRES_NONE);
G_STATIC_ASSERT ((guint) GST_AV1_ENC_SUPERRES_FIXED == (guint) AOM_SUPERRES_FIXED);
G_STATIC_ASSERT ((guint) GST_AV1_ENC_SUPERRES_RANDOM == (guint) AOM_SUPERRES_RANDOM);
G_STATIC_ASSERT ((guint) GST_AV1_ENC_SUPERRES_QTHRESH == (guint) AOM_SUPERRES_QTHRESH);
#endif

/**
 * GstAV1EncEndUsageMode:
 * @GST_AV1_ENC_END_USAGE_VBR: Variable Bit Rate Mode
 * @GST_AV1_ENC_END_USAGE_CBR: Constant Bit Rate Mode
 * @GST_AV1_ENC_END_USAGE_CQ: Constrained Quality Mode
 * @GST_AV1_ENC_END_USAGE_Q: Constant Quality Mode
 *
 * Rate control algorithm to use
 */

typedef enum
{
  GST_AV1_ENC_END_USAGE_VBR = 0,
  GST_AV1_ENC_END_USAGE_CBR = 1,
  GST_AV1_ENC_END_USAGE_CQ = 2,
  GST_AV1_ENC_END_USAGE_Q = 3,
} GstAV1EncEndUsageMode;

G_STATIC_ASSERT ((guint) GST_AV1_ENC_END_USAGE_VBR == (guint) AOM_VBR);
G_STATIC_ASSERT ((guint) GST_AV1_ENC_END_USAGE_CBR == (guint) AOM_CBR);
G_STATIC_ASSERT ((guint) GST_AV1_ENC_END_USAGE_CQ == (guint) AOM_CQ);
G_STATIC_ASSERT ((guint) GST_AV1_ENC_END_USAGE_Q == (guint) AOM_Q);

/**
 * GstAV1EncKFMode:
 * @GST_AV1_ENC_KF_DISABLED: Encoder does not place keyframes
 * @GST_AV1_ENC_KF_AUTO: Encoder determines optimal keyframe placement automatically
 *
 * Determines whether keyframes are placed automatically by the encoder
 *
 * Since: 1.22
 */

typedef enum
{
  GST_AV1_ENC_KF_DISABLED = 0,
  GST_AV1_ENC_KF_AUTO = 1,
} GstAV1EncKFMode;

G_STATIC_ASSERT ((guint) GST_AV1_ENC_KF_DISABLED == (guint) AOM_KF_DISABLED);
G_STATIC_ASSERT ((guint) GST_AV1_ENC_KF_AUTO == (guint) AOM_KF_AUTO);

/**
 * GstAV1EncEncPass:
 * @GST_AV1_ENC_ONE_PASS: Single pass mode
 * @GST_AV1_ENC_FIRST_PASS: First pass of multi-pass mode
 * @GST_AV1_ENC_SECOND_PASS: Second pass of multi-pass mode
 * @GST_AV1_ENC_THIRD_PASS: Third pass of multi-pass mode
 * Current phase for multi-pass encoding or @GST_AV1_ENC_ONE_PASS for single pass
 *
 * Since: 1.22
 */

typedef enum
{
  GST_AV1_ENC_ONE_PASS = 0,
  GST_AV1_ENC_FIRST_PASS = 1,
  GST_AV1_ENC_SECOND_PASS = 2,
  GST_AV1_ENC_THIRD_PASS = 3,
} GstAV1EncEncPass;

#ifdef HAVE_LIBAOM_3_2
G_STATIC_ASSERT ((guint) GST_AV1_ENC_ONE_PASS == (guint) AOM_RC_ONE_PASS);
G_STATIC_ASSERT ((guint) GST_AV1_ENC_FIRST_PASS == (guint) AOM_RC_FIRST_PASS);
G_STATIC_ASSERT ((guint) GST_AV1_ENC_SECOND_PASS == (guint) AOM_RC_SECOND_PASS);
G_STATIC_ASSERT ((guint) GST_AV1_ENC_THIRD_PASS == (guint) AOM_RC_THIRD_PASS);
#endif

/**
 * GstAV1EncUsageProfile:
 * @GST_AV1_ENC_USAGE_GOOD_QUALITY: Good Quality profile
 * @GST_AV1_ENC_USAGE_REALTIME: Realtime profile
 * @GST_AV1_ENC_USAGE_ALL_INTRA: All Intra profile
 *
 * Usage profile is used to guide the default config for the encoder
 *
 * Since: 1.22
 */

typedef enum
{
  GST_AV1_ENC_USAGE_GOOD_QUALITY = 0,
  GST_AV1_ENC_USAGE_REALTIME = 1,
  GST_AV1_ENC_USAGE_ALL_INTRA = 2,
} GstAV1EncUsageProfile;

struct _GstAV1Enc
{
  GstVideoEncoder base_video_encoder;

  /* properties */
  gint cpu_used;
  gint threads;
  gboolean row_mt;
  guint tile_columns;
  guint tile_rows;

  /* state */
  gboolean encoder_inited;
  GstVideoCodecState *input_state;
  aom_codec_enc_cfg_t aom_cfg;
  aom_codec_ctx_t encoder;
  aom_img_fmt_t format;
  GMutex encoder_lock;

  /* Last inputi pts, in running time */
  GstClockTime last_pts;
  /* duration of the last input buffer */
  GstClockTime last_input_duration;

  gboolean target_bitrate_set;
};

struct _GstAV1EncClass
{
  GstVideoEncoderClass parent_class;
  /*supported aom algo*/
  aom_codec_iface_t *codec_algo;
};

GType gst_av1_enc_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (av1enc);

G_END_DECLS
#endif /* __GST_AV1_ENC_H__ */
