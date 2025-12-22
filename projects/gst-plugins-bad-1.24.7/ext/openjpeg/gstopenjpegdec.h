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

#ifndef __GST_OPENJPEG_DEC_H__
#define __GST_OPENJPEG_DEC_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/codecparsers/gstjpeg2000sampling.h>

#include "gstopenjpeg.h"

G_BEGIN_DECLS

#define GST_TYPE_OPENJPEG_DEC \
  (gst_openjpeg_dec_get_type())
#define GST_OPENJPEG_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OPENJPEG_DEC,GstOpenJPEGDec))
#define GST_OPENJPEG_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OPENJPEG_DEC,GstOpenJPEGDecClass))
#define GST_IS_OPENJPEG_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OPENJPEG_DEC))
#define GST_IS_OPENJPEG_DEC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OPENJPEG_DEC))

typedef struct _GstOpenJPEGDec GstOpenJPEGDec;
typedef struct _GstOpenJPEGDecClass GstOpenJPEGDecClass;

struct _GstOpenJPEGDec
{
  GstVideoDecoder parent;

  /* < private > */
  GstVideoCodecState *input_state;
  GstVideoCodecState *output_state;

  OPJ_CODEC_FORMAT codec_format;
  gboolean is_jp2c;
  OPJ_COLOR_SPACE color_space;
  GstJPEG2000Sampling sampling;
  gint ncomps;
  gint max_threads;  /* atomic */
  gint max_slice_threads; /* internal openjpeg threading system */
  gint num_procs;
  gint num_stripes;
  gboolean drop_subframes;

  void (*fill_frame) (GstOpenJPEGDec *self,
                      GstVideoFrame *frame, opj_image_t * image);

  GstFlowReturn (*decode_frame) (GstVideoDecoder * decoder, GstVideoCodecFrame *frame);

  opj_dparameters_t params;

  guint available_threads;
  GQueue messages;

  GCond messages_cond;
  GMutex messages_lock;
  GMutex decoding_lock;
  GstFlowReturn downstream_flow_ret;
  gboolean flushing;

  /* Draining state */
  GMutex drain_lock;
  GCond drain_cond;
  /* TRUE if EOS buffers shouldn't be forwarded */
  gboolean draining; /* protected by drain_lock */

  int last_error;

  gboolean started;
};

struct _GstOpenJPEGDecClass
{
  GstVideoDecoderClass parent_class;
};

GType gst_openjpeg_dec_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (openjpegdec);

G_END_DECLS

#endif /* __GST_OPENJPEG_DEC_H__ */
