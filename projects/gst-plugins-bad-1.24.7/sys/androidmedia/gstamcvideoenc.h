/*
 * Copyright (C) 2012, Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *
 * Copyright (C) 2013, Lemote Ltd.
 *   Author: Chen Jie <chenj@lemote.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifndef __GST_AMC_VIDEO_ENC_H__
#define __GST_AMC_VIDEO_ENC_H__

#include <gst/gst.h>

#include <gst/video/gstvideoencoder.h>

#include "gstamc.h"

G_BEGIN_DECLS

#define GST_TYPE_AMC_VIDEO_ENC \
  (gst_amc_video_enc_get_type())
#define GST_AMC_VIDEO_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AMC_VIDEO_ENC,GstAmcVideoEnc))
#define GST_AMC_VIDEO_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AMC_VIDEO_ENC,GstAmcVideoEncClass))
#define GST_AMC_VIDEO_ENC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_AMC_VIDEO_ENC,GstAmcVideoEncClass))
#define GST_IS_AMC_VIDEO_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AMC_VIDEO_ENC))
#define GST_IS_AMC_VIDEO_ENC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AMC_VIDEO_ENC))
typedef struct _GstAmcVideoEnc GstAmcVideoEnc;
typedef struct _GstAmcVideoEncClass GstAmcVideoEncClass;

struct _GstAmcVideoEnc
{
  GstVideoEncoder parent;

  /* < private > */
  GMutex codec_lock; // Protect creation / destruction of the codec
  GstAmcCodec *codec;
  GstAmcFormat *amc_format;

  /* Set to TRUE if codec headers should be placed
   * in the stream, or FALSE if they go in the headers */
  gboolean codec_data_in_bytestream;

  GstVideoCodecState *input_state;

  /* Input format of the codec */
  GstVideoFormat format;
  GstAmcColorFormatInfo color_format_info;

  guint bitrate;
  gfloat i_frame_int;

  /* TRUE if the component is configured and saw
   * the first buffer */
  gboolean started;
  gboolean flushing;

  GstClockTime last_upstream_ts;

  /* Draining state */
  GMutex drain_lock;
  GCond drain_cond;
  /* TRUE if EOS buffers shouldn't be forwarded */
  gboolean draining;
  /* TRUE if the component is drained */
  gboolean drained;

  GstFlowReturn downstream_flow_ret;
};

struct _GstAmcVideoEncClass
{
  GstVideoEncoderClass parent_class;

  const GstAmcCodecInfo *codec_info;
};

GType gst_amc_video_enc_get_type (void);

G_END_DECLS

#endif /* __GST_AMC_VIDEO_ENC_H__ */
