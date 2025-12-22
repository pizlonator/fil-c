/* 
 * Copyright (C) 2012 Collabora Ltd.
 *     Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
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
 *
 */

#ifndef __GST_OPENJPEG_ENC_H__
#define __GST_OPENJPEG_ENC_H__

#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstopenjpeg.h"

G_BEGIN_DECLS

#define GST_TYPE_OPENJPEG_ENC \
  (gst_openjpeg_enc_get_type())
#define GST_OPENJPEG_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OPENJPEG_ENC,GstOpenJPEGEnc))
#define GST_OPENJPEG_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OPENJPEG_ENC,GstOpenJPEGEncClass))
#define GST_IS_OPENJPEG_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OPENJPEG_ENC))
#define GST_IS_OPENJPEG_ENC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OPENJPEG_ENC))

typedef struct _GstOpenJPEGEnc GstOpenJPEGEnc;
typedef struct _GstOpenJPEGEncClass GstOpenJPEGEncClass;

struct _GstOpenJPEGEnc
{
  GstVideoEncoder parent;

  /* < private > */
  GstVideoCodecState *input_state;
  GstVideoCodecState *output_state;

  OPJ_CODEC_FORMAT codec_format;
  gboolean is_jp2c;

  void (*fill_image) (opj_image_t * image, GstVideoFrame *frame);
  GstFlowReturn (*encode_frame) (GstVideoEncoder * encoder, GstVideoCodecFrame *frame);

  opj_cparameters_t params;
  gint num_stripes;

  guint available_threads;
  GQueue messages;

  GCond messages_cond;

  OpenJPEGErrorCode last_error;
};

struct _GstOpenJPEGEncClass
{
  GstVideoEncoderClass parent_class;
};

GType gst_openjpeg_enc_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (openjpegenc);

G_END_DECLS

#endif /* __GST_OPENJPEG_ENC_H__ */
