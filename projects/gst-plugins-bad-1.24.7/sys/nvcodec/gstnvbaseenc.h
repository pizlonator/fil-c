/* GStreamer NVENC plugin
 * Copyright (C) 2015 Centricular Ltd
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

#ifndef __GST_NV_BASE_ENC_H_INCLUDED__
#define __GST_NV_BASE_ENC_H_INCLUDED__

#include "gstnvenc.h"

#include <gst/video/gstvideoencoder.h>
#include <gst/cuda/gstcuda.h>

#define GST_TYPE_NV_BASE_ENC \
  (gst_nv_base_enc_get_type())
#define GST_NV_BASE_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_NV_BASE_ENC,GstNvBaseEnc))
#define GST_NV_BASE_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_NV_BASE_ENC,GstNvBaseEncClass))
#define GST_NV_BASE_ENC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_NV_BASE_ENC,GstNvBaseEncClass))
#define GST_IS_NV_BASE_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_NV_BASE_ENC))
#define GST_IS_NV_BASE_ENC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_NV_BASE_ENC))

typedef enum {
  GST_NV_PRESET_DEFAULT,
  GST_NV_PRESET_HP,
  GST_NV_PRESET_HQ,
/* FIXME: problematic GST_NV_PRESET_BD, */
  GST_NV_PRESET_LOW_LATENCY_DEFAULT,
  GST_NV_PRESET_LOW_LATENCY_HQ,
  GST_NV_PRESET_LOW_LATENCY_HP,
  GST_NV_PRESET_LOSSLESS_DEFAULT,
  GST_NV_PRESET_LOSSLESS_HP,
} GstNvPreset;

typedef enum {
  GST_NV_RC_MODE_DEFAULT,
  GST_NV_RC_MODE_CONSTQP,
  GST_NV_RC_MODE_CBR,
  GST_NV_RC_MODE_VBR,
  GST_NV_RC_MODE_VBR_MINQP,
  GST_NV_RC_MODE_CBR_LOWDELAY_HQ,
  GST_NV_RC_MODE_CBR_HQ,
  GST_NV_RC_MODE_VBR_HQ,
} GstNvRCMode;

typedef enum
{
  GST_NVENC_MEM_TYPE_SYSTEM = 0,
  GST_NVENC_MEM_TYPE_GL,
  GST_NVENC_MEM_TYPE_CUDA,
  /* FIXME: add support D3D11 memory */
} GstNvEncMemType;

typedef struct {
  gboolean weighted_prediction;
  gint rc_modes;
  gboolean custom_vbv_bufsize;
  gboolean lookahead;
  gboolean temporal_aq;
  gint bframes;
} GstNvEncDeviceCaps;

typedef struct {
  gint qp_i;
  gint qp_p;
  gint qp_b;
} GstNvEncQP;

typedef struct {
  GstVideoEncoder video_encoder;

  /* properties */
  GstNvPreset     preset_enum;
  GUID            selected_preset;
  GstNvRCMode     rate_control_mode;
  gint            qp_min;
  GstNvEncQP      qp_min_detail;
  gint            qp_max;
  GstNvEncQP      qp_max_detail;
  gint            qp_const;
  GstNvEncQP      qp_const_detail;
  guint           bitrate;
  gint            gop_size;
  guint           max_bitrate;
  gboolean        spatial_aq;
  guint           aq_strength;
  gboolean        non_refp;
  /* zero reorder delay (consistent naming with x264) */
  gboolean        zerolatency;
  gboolean        strict_gop;
  gdouble         const_quality;
  gboolean        i_adapt;

  GstCudaContext * cuda_ctx;
  GstCudaStream * stream;
  void          * encoder;
  NV_ENC_INITIALIZE_PARAMS init_params;
  NV_ENC_CONFIG            config;

  /* the supported input formats */
  GValue        * input_formats;                  /* OBJECT LOCK */

  GstVideoCodecState *input_state;
  gint                reconfig;                   /* ATOMIC */
  GstNvEncMemType     mem_type;

  /* array of allocated input/output buffers (GstNvEncFrameState),
   * and hold the ownership of the GstNvEncFrameState. */
  GArray            *items;

  /* (GstNvEncFrameState) available empty items which could be submitted
   * to encoder */
  GAsyncQueue       *available_queue;

  /* (GstNvEncFrameState) submitted to encoder but not ready to finish
   * (due to bframe or lookhead operation) */
  GAsyncQueue       *pending_queue;

  /* (GstNvEncFrameState) submitted to encoder and ready to finish.
   * finished items will go back to available item queue */
  GAsyncQueue       *bitstream_queue;

  /* we spawn a thread that does the (blocking) waits for output buffers
   * to become available, so we can continue to feed data to the encoder
   * while we wait */
  GThread        *bitstream_thread;

  GstObject      *display;            /* GstGLDisplay */
  GstObject      *other_context;      /* GstGLContext */
  GstObject      *gl_context;         /* GstGLContext */

  GstVideoInfo        input_info;     /* buffer configuration for buffers sent to NVENC */

  GstFlowReturn   last_flow;          /* ATOMIC */

  /* the first frame when bframe was enabled */
  GstVideoCodecFrame *first_frame;
  GstClockTime dts_offset;

  /*< protected >*/
  /* device capability dependent properties, set by subclass */
  gboolean        weighted_pred;
  guint           vbv_buffersize;
  guint           rc_lookahead;
  gboolean        temporal_aq;
  guint           bframes;
  gboolean        b_adapt;
} GstNvBaseEnc;

typedef struct {
  GstVideoEncoderClass video_encoder_class;

  GUID codec_id;
  guint cuda_device_id;
  GstNvEncDeviceCaps device_caps;

  gboolean (*set_src_caps)       (GstNvBaseEnc * nvenc,
                                  GstVideoCodecState * state);
  gboolean (*set_pic_params)     (GstNvBaseEnc * nvenc,
                                  GstVideoCodecFrame * frame,
                                  NV_ENC_PIC_PARAMS * pic_params);
  gboolean (*set_encoder_config) (GstNvBaseEnc * nvenc,
                                  GstVideoCodecState * state,
                                  NV_ENC_CONFIG * config);
} GstNvBaseEncClass;

GType gst_nv_base_enc_get_type (void);

GType gst_nv_base_enc_register                (const char * codec,
                                               guint device_id,
                                               GstNvEncDeviceCaps * device_caps);

void gst_nv_base_enc_schedule_reconfig        (GstNvBaseEnc * nvenc);


#endif /* __GST_NV_BASE_ENC_H_INCLUDED__ */
