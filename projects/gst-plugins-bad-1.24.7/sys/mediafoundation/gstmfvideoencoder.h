/* GStreamer
 * Copyright (C) 2020 Seungha Yang <seungha.yang@navercorp.com>
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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

#include "gstmfconfig.h"

#include <gst/gst.h>
#include <gst/video/video.h>
#include "gstmfutils.h"
#include "gstmftransform.h"

#if GST_MF_HAVE_D3D11
#include <gst/d3d11/gstd3d11.h>
#endif

G_BEGIN_DECLS

#define GST_TYPE_MF_VIDEO_ENCODER           (gst_mf_video_encoder_get_type())
#define GST_MF_VIDEO_ENCODER(obj)           (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MF_VIDEO_ENCODER,GstMFVideoEncoder))
#define GST_MF_VIDEO_ENCODER_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_MF_VIDEO_ENCODER,GstMFVideoEncoderClass))
#define GST_MF_VIDEO_ENCODER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_MF_VIDEO_ENCODER,GstMFVideoEncoderClass))
#define GST_IS_MF_VIDEO_ENCODER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MF_VIDEO_ENCODER))
#define GST_IS_MF_VIDEO_ENCODER_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_MF_VIDEO_ENCODER))

typedef struct _GstMFVideoEncoder GstMFVideoEncoder;
typedef struct _GstMFVideoEncoderClass GstMFVideoEncoderClass;
typedef struct _GstMFVideoEncoderDeviceCaps GstMFVideoEncoderDeviceCaps;
typedef struct _GstMFVideoEncoderClassData GstMFVideoEncoderClassData;

struct _GstMFVideoEncoderDeviceCaps
{
  gboolean rc_mode; /* AVEncCommonRateControlMode */
  gboolean quality; /* AVEncCommonQuality */

  gboolean adaptive_mode;     /* AVEncAdaptiveMode */
  gboolean buffer_size;       /* AVEncCommonBufferSize */
  gboolean mean_bitrate;      /* AVEncCommonMeanBitRate */
  gboolean max_bitrate;       /* AVEncCommonMaxBitRate */
  gboolean quality_vs_speed;  /* AVEncCommonQualityVsSpeed */
  gboolean cabac;             /* AVEncH264CABACEnable */
  gboolean sps_id;            /* AVEncH264SPSID */
  gboolean pps_id;            /* AVEncH264PPSID */
  gboolean bframes;           /* AVEncMPVDefaultBPictureCount */
  gboolean gop_size;          /* AVEncMPVGOPSize */
  gboolean threads;           /* AVEncNumWorkerThreads */
  gboolean content_type;      /* AVEncVideoContentType */
  gboolean qp;                /* AVEncVideoEncodeQP */
  gboolean force_keyframe;    /* AVEncVideoForceKeyFrame */
  gboolean low_latency;       /* AVLowLatencyMode */

  gboolean min_qp;        /* AVEncVideoMinQP */
  gboolean max_qp;        /* AVEncVideoMaxQP */
  gboolean frame_type_qp; /* AVEncVideoEncodeFrameTypeQP */
  gboolean max_num_ref;   /* AVEncVideoMaxNumRefFrame */
  guint max_num_ref_high;
  guint max_num_ref_low;

  /* TRUE if MFT support d3d11 and also we can use d3d11 interop */
  gboolean d3d11_aware;
  /* DXGI adapter LUID, valid only when d3d11_aware == TRUE */
  gint64 adapter_luid;
};

struct _GstMFVideoEncoderClassData
{
  GstCaps *sink_caps;
  GstCaps *src_caps;
  gchar *device_name;
  guint32 enum_flags;
  guint device_index;
  GstMFVideoEncoderDeviceCaps device_caps;
  gboolean is_default;
};

struct _GstMFVideoEncoder
{
  GstVideoEncoder parent;

  GstMFTransform *transform;
  gboolean async_mft;
  GstFlowReturn last_ret;

  GstVideoCodecState *input_state;

  /* Set by subclass */
  gboolean has_reorder_frame;

  /* Calculated timestamp offset in MF timescale (100ns scale)
   * when B-frame is enabled. */
  LONGLONG mf_pts_offset;

  gboolean need_align;

#if GST_MF_HAVE_D3D11
  /* For D3D11 interop. */
  GstD3D11Device *other_d3d11_device;
  GstD3D11Device *d3d11_device;
  IMFDXGIDeviceManager *device_manager;
  UINT reset_token;
  IMFVideoSampleAllocatorEx *mf_allocator;
  GstD3D11Fence *fence;
#endif
};

struct _GstMFVideoEncoderClass
{
  GstVideoEncoderClass parent_class;

  /* Set by subclass */
  GUID codec_id;      /* Output subtype of MFT */
  guint32 enum_flags; /* MFT_ENUM_FLAG */
  guint device_index; /* Index of enumerated IMFActivate via MFTEnum */
  GstMFVideoEncoderDeviceCaps device_caps;

  gboolean (*set_option)    (GstMFVideoEncoder * encoder,
                             GstVideoCodecState * state,
                             IMFMediaType * output_type);

  gboolean (*set_src_caps)  (GstMFVideoEncoder * encoder,
                             GstVideoCodecState * state,
                             IMFMediaType * output_type);

  gboolean (*check_reconfigure) (GstMFVideoEncoder * encoder);
};

GType gst_mf_video_encoder_get_type (void);

void  gst_mf_video_encoder_register (GstPlugin * plugin,
                                     guint rank,
                                     GUID * subtype,
                                     GTypeInfo * type_info,
                                     GList * d3d11_device);

G_END_DECLS
