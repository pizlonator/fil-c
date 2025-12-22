/* GStreamer H265 encoder plugin
 * Copyright (C) 2019 Yeongjin Jeong <yeongjin.jeong@navercorp.com>
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

#ifndef __GST_SVTHEVC_ENC_H__
#define __GST_SVTHEVC_ENC_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideoencoder.h>

#include <EbApi.h>

G_BEGIN_DECLS
#define GST_TYPE_SVTHEVC_ENC \
  (gst_svthevc_enc_get_type())
G_DECLARE_FINAL_TYPE (GstSvtHevcEnc, gst_svthevc_enc, GST, SVTHEVC_ENC, GstVideoEncoder)
#define GST_SVTHEVC_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SVTHEVC_ENC,GstSvtHevcEncClass))
#define GST_IS_SVTHEVC_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SVTHEVC_ENC))

typedef enum svt_eos_status
{
  EOS_NOT_REACHED = 0,
  EOS_REACHED,
  EOS_TOTRIGGER
} SVT_EOS_STATUS;

typedef enum
{
  GST_SVTHEVC_ENC_B_PYRAMID_FLAT,
  GST_SVTHEVC_ENC_B_PYRAMID_2LEVEL_HIERARCHY,
  GST_SVTHEVC_ENC_B_PYRAMID_3LEVEL_HIERARCHY,
  GST_SVTHEVC_ENC_B_PYRAMID_4LEVEL_HIERARCHY,
} GstSvtHevcEncBPyramid;

typedef enum
{
  GST_SVTHEVC_ENC_BASE_LAYER_MODE_BFRAME,
  GST_SVTHEVC_ENC_BASE_LAYER_MODE_PFRAME,
} GstSvtHevcEncBaseLayerMode;

typedef enum
{
  GST_SVTHEVC_ENC_RC_CQP,
  GST_SVTHEVC_ENC_RC_VBR,
} GstSvtHevcEncRC;

typedef enum
{
  GST_SVTHEVC_ENC_TUNE_SQ,
  GST_SVTHEVC_ENC_TUNE_OQ,
  GST_SVTHEVC_ENC_TUNE_VMAF,
} GstSvtHevcEncTune;

typedef enum
{
  GST_SVTHEVC_ENC_PRED_STRUCT_LOW_DELAY_P,
  GST_SVTHEVC_ENC_PRED_STRUCT_LOW_DELAY_B,
  GST_SVTHEVC_ENC_PRED_STRUCT_RANDOM_ACCESS,
} GstSvtHevcEncPredStruct;

struct _GstSvtHevcEnc
{
  GstVideoEncoder element;

  /*< private > */
  const gchar *svthevc_version;
  EB_H265_ENC_CONFIGURATION enc_params;
  EB_COMPONENTTYPE *svt_handle;

  EB_BUFFERHEADERTYPE *in_buf;

  SVT_EOS_STATUS svt_eos_flag;

  GstClockTime dts_offset;
  GstVideoCodecFrame *first_frame;
  gboolean push_header;
  gboolean first_buffer;
  gboolean update_latency;

  /* Internally used for convert stride to multiple of pstride */
  GstBufferPool *internal_pool;
  GstVideoInfo *aligned_info;

  /* properties */
  gboolean insert_vui;
  gboolean aud;
  GstSvtHevcEncBPyramid hierarchical_level;
  guint la_depth;
  guint enc_mode;
  GstSvtHevcEncRC rc_mode;
  guint qp_i;
  guint qp_max;
  guint qp_min;
  gboolean scene_change_detection;
  GstSvtHevcEncTune tune;
  GstSvtHevcEncBaseLayerMode base_layer_switch_mode;
  guint bitrate;
  gint keyintmax;
  gboolean enable_open_gop;
  guint config_interval;
  guint cores;
  gint socket;
  guint tile_row;
  guint tile_col;
  GstSvtHevcEncPredStruct pred_structure;
  guint vbv_maxrate;
  guint vbv_bufsize;

  guint profile;
  guint tier;
  guint level;

  /* input description */
  GstVideoCodecState *input_state;

  /* configuration changed  while playing */
  gboolean reconfig;
};

G_END_DECLS
#endif /* __GST_SVTHEVC_ENC_H__ */
