/*
 * Copyright (c) 2014, Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

/*/
\*\ GstOpenh264Enc
/*/

#ifndef _GST_OPENH264ENC_H_
#define _GST_OPENH264ENC_H_

#include <gst/video/video.h>
#include <gst/video/gstvideoencoder.h>

#include <wels/codec_api.h>
#include <wels/codec_app_def.h>
#include <wels/codec_def.h>
#include <wels/codec_ver.h>

G_BEGIN_DECLS

typedef enum _GstOpenh264encDeblockingMode
{
  GST_OPENH264_DEBLOCKING_ON = 0,
  GST_OPENH264_DEBLOCKING_OFF = 1,
  GST_OPENH264_DEBLOCKING_NOT_SLICE_BOUNDARIES = 2
} GstOpenh264encDeblockingMode;

typedef enum
{
  GST_OPENH264_SLICE_MODE_N_SLICES = 1,  /* SM_FIXEDSLCNUM_SLICE */
  GST_OPENH264_SLICE_MODE_AUTO = 5       /* former SM_AUTO_SLICE */
} GstOpenh264EncSliceMode;

#define GST_TYPE_OPENH264ENC          (gst_openh264enc_get_type())
#define GST_OPENH264ENC(obj)          (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OPENH264ENC,GstOpenh264Enc))
#define GST_OPENH264ENC_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OPENH264ENC,GstOpenh264EncClass))
#define GST_IS_OPENH264ENC(obj)       (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OPENH264ENC))
#define GST_IS_OPENH264ENC_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OPENH264ENC))

typedef struct _GstOpenh264Enc GstOpenh264Enc;
typedef struct _GstOpenh264EncClass GstOpenh264EncClass;

struct _GstOpenh264Enc
{
  GstVideoEncoder base_openh264enc;

  /*< private >*/
  ISVCEncoder *encoder;
  EUsageType usage_type;
  guint gop_size;
  RC_MODES rate_control;
  guint max_slice_size;
  guint bitrate;
  guint max_bitrate;
  guint qp_min;
  guint qp_max;
  guint framerate;
  guint multi_thread;
  gboolean enable_denoise;
  gboolean enable_frame_skip;
  GstVideoCodecState *input_state;
  guint64 time_per_frame;
  guint64 frame_count;
  guint64 previous_timestamp;
  GstOpenh264encDeblockingMode deblocking_mode;
  gboolean background_detection;
  gboolean adaptive_quantization;
  gboolean scene_change_detection;
  GstOpenh264EncSliceMode slice_mode;
  guint num_slices;
  ECOMPLEXITY_MODE complexity;
  gboolean bitrate_changed;
  gboolean max_bitrate_changed;
};

struct _GstOpenh264EncClass
{
    GstVideoEncoderClass base_openh264enc_class;
};

GType gst_openh264enc_get_type(void);

G_END_DECLS
#endif
