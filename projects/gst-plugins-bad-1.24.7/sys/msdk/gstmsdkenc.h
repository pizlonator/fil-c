/* GStreamer Intel MSDK plugin
 * Copyright (c) 2016, Oblong Industries, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __GST_MSDKENC_H__
#define __GST_MSDKENC_H__

#include <gst/gst.h>
#include <gst/video/gstvideoencoder.h>
#include "msdk.h"
#include "msdk-enums.h"
#include "gstmsdkcontext.h"
#include "gstmsdkcaps.h"

G_BEGIN_DECLS

#define GST_TYPE_MSDKENC \
  (gst_msdkenc_get_type())
#define GST_MSDKENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MSDKENC,GstMsdkEnc))
#define GST_MSDKENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MSDKENC,GstMsdkEncClass))
#define GST_MSDKENC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_MSDKENC,GstMsdkEncClass))
#define GST_IS_MSDKENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MSDKENC))
#define GST_IS_MSDKENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MSDKENC))

#define MAX_EXTRA_PARAMS 8

#define check_update_property(type, obj, old_val, new_val)          \
  gst_msdkenc_check_update_property_##type (obj, old_val, new_val)
#define check_update_property_uint(obj, old_val, new_val)           \
  check_update_property (uint, obj, old_val, new_val)
#define check_update_property_int(obj, old_val, new_val)            \
  check_update_property (int, obj, old_val, new_val)
#define check_update_property_bool(obj, old_val, new_val)           \
  check_update_property (bool, obj, old_val, new_val)

typedef struct _GstMsdkEnc GstMsdkEnc;
typedef struct _GstMsdkEncClass GstMsdkEncClass;
typedef struct _MsdkEncTask MsdkEncTask;
typedef struct _MsdkEncCData MsdkEncCData;

enum
{
  GST_MSDKENC_PROP_0,
  GST_MSDKENC_PROP_HARDWARE,
  GST_MSDKENC_PROP_ASYNC_DEPTH,
  GST_MSDKENC_PROP_TARGET_USAGE,
  GST_MSDKENC_PROP_RATE_CONTROL,
  GST_MSDKENC_PROP_BITRATE,
  GST_MSDKENC_PROP_MAX_FRAME_SIZE,
  GST_MSDKENC_PROP_MAX_VBV_BITRATE,
  GST_MSDKENC_PROP_AVBR_ACCURACY,
  GST_MSDKENC_PROP_AVBR_CONVERGENCE,
  GST_MSDKENC_PROP_RC_LOOKAHEAD_DEPTH,
  GST_MSDKENC_PROP_QPI,
  GST_MSDKENC_PROP_QPP,
  GST_MSDKENC_PROP_QPB,
  GST_MSDKENC_PROP_GOP_SIZE,
  GST_MSDKENC_PROP_REF_FRAMES,
  GST_MSDKENC_PROP_I_FRAMES,
  GST_MSDKENC_PROP_B_FRAMES,
  GST_MSDKENC_PROP_NUM_SLICES,
  GST_MSDKENC_PROP_MBBRC,
  GST_MSDKENC_PROP_ADAPTIVE_I,
  GST_MSDKENC_PROP_ADAPTIVE_B,
  GST_MSDKENC_PROP_EXT_CODING_PROPS,
  GST_MSDKENC_PROP_LOWDELAY_BRC,
  GST_MSDKENC_PROP_MAX_FRAME_SIZE_I,
  GST_MSDKENC_PROP_MAX_FRAME_SIZE_P,
  GST_MSDKENC_PROP_MAX,
};

struct _GstMsdkEnc
{
  GstVideoEncoder element;

  /* input description */
  GstVideoCodecState *input_state;

  /* List of frame/buffer mapping structs for
   * pending frames */
  GList *pending_frames;

  /* MFX context */
  GstMsdkContext *context;
  GstMsdkContext *old_context;
  mfxVideoParam param;
  guint num_tasks;
  MsdkEncTask *tasks;
  guint next_task;
  /* Extra frames for encoding, set by each element,
   * the default value is 0 */
  guint num_extra_frames;

  mfxExtBuffer *extra_params[MAX_EXTRA_PARAMS];
  guint num_extra_params;

  /* Additional encoder coding options */
  mfxExtCodingOption2 option2;
  mfxExtCodingOption3 option3;
  gboolean enable_extopt3;

  /* parameters for per-frame based encoding control */
  mfxEncodeCtrl enc_cntrl;

  GstBufferPool *msdk_pool;
  GstBufferPool *msdk_converted_pool;
  GstVideoInfo aligned_info;
  gboolean use_video_memory;
  gboolean use_dmabuf;
  gboolean use_va;
  gboolean use_d3d11;
  gboolean initialized;
  guint64 modifier;

  /* element properties */
  gboolean hardware;

  guint async_depth;
  guint target_usage;
  guint rate_control;
  guint bitrate;
  guint max_frame_size;
  guint max_vbv_bitrate;
  guint accuracy;
  guint convergence;
  guint lookahead_depth;
  guint qpi;
  guint qpp;
  guint qpb;
  guint gop_size;
  guint ref_frames;
  guint i_frames;
  gint b_frames;
  guint num_slices;
  gint16 mbbrc;
  gint16 adaptive_i;
  gint16 adaptive_b;
  guint max_frame_size_i;
  guint max_frame_size_p;
  gint16 lowdelay_brc;

  GstClockTime start_pts;
  GstClockTime frame_duration;

  GstStructure *ext_coding_props;

  gboolean reconfig;

  guint16 codename;
};

struct _GstMsdkEncClass
{
  GstVideoEncoderClass parent_class;

  gboolean (*set_format) (GstMsdkEnc * encoder);
  gboolean (*configure) (GstMsdkEnc * encoder);
  GstCaps *(*set_src_caps) (GstMsdkEnc * encoder);
  gboolean (*is_format_supported) (GstMsdkEnc * encoder, GstVideoFormat format);

  /* Return TRUE if sub class requires a recofnig */
  gboolean (*need_reconfig) (GstMsdkEnc * encoder, GstVideoCodecFrame * frame);

  /* Allow sub class set extra frame parameters */
  void (*set_extra_params) (GstMsdkEnc * encoder, GstVideoCodecFrame * frame);

  guint qp_max;
  guint qp_min;
};

struct _MsdkEncTask
{
  mfxSyncPoint sync_point;
  mfxBitstream output_bitstream;
};

struct _MsdkEncCData
{
  GstCaps *sink_caps;
  GstCaps *src_caps;
};

GType gst_msdkenc_get_type (void);

void gst_msdkenc_add_extra_param (GstMsdkEnc * thiz, mfxExtBuffer * param);

void
gst_msdkenc_install_common_properties (GstMsdkEncClass *encoder_class);

gboolean
gst_msdkenc_check_update_property_uint (GstMsdkEnc * thiz, guint * old_val,
                                        guint new_val);

gboolean
gst_msdkenc_check_update_property_int (GstMsdkEnc * thiz, gint * old_val,
                                        gint new_val);

gboolean
gst_msdkenc_check_update_property_bool (GstMsdkEnc * thiz, gboolean * old_val,
                                        gboolean new_val);

gboolean
gst_msdkenc_set_common_property (GObject * object, guint prop_id,
                                 const GValue * value, GParamSpec * pspec);
gboolean
gst_msdkenc_get_common_property (GObject * object, guint prop_id,
                                 GValue * value, GParamSpec * pspec);
void
gst_msdkenc_ensure_extended_coding_options (GstMsdkEnc * thiz);

gboolean
gst_msdkenc_get_roi_params (GstMsdkEnc * thiz,
    GstVideoCodecFrame * frame, mfxExtEncoderROI * encoder_roi);
G_END_DECLS

#endif /* __GST_MSDKENC_H__ */
