/* GStreamer
 * Copyright (C) 2019 Julien Isorce <julien.isorce@gmail.com>
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


#ifndef __GST_FAKE_VIDEO_DEC_H__
#define __GST_FAKE_VIDEO_DEC_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/gstvideodecoder.h>
#include "gstdebugutilselements.h"

G_BEGIN_DECLS
#define GST_TYPE_FAKE_VIDEO_DEC \
  (gst_fake_video_dec_get_type())
#define GST_FAKE_VIDEO_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_FAKE_VIDEO_DEC,GstFakeVideoDec))
#define GST_FAKE_VIDEO_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_FAKE_VIDEO_DEC,GstFakeVideoDecClass))
#define GST_IS_FAKE_VIDEO_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_FAKE_VIDEO_DEC))
#define GST_IS_FAKE_VIDEO_DEC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_FAKE_VIDEO_DEC))
typedef struct _GstFakeVideoDec GstFakeVideoDec;
typedef struct _GstFakeVideoDecClass GstFakeVideoDecClass;

struct _GstFakeVideoDec
{
  GstVideoDecoder base_video_decoder;

  GstVideoCodecState *input_state;
  GstVideoCodecState *output_state;

  guint min_buffers;
  guint snake_current_step;
  guint snake_max_steps;
  guint snake_length;
};

struct _GstFakeVideoDecClass
{
  GstVideoDecoderClass base_video_decoder_class;
};

GType gst_fake_video_dec_get_type (void);

G_END_DECLS
#endif /* __GST_FAKE_VIDEO_DEC_H__ */
