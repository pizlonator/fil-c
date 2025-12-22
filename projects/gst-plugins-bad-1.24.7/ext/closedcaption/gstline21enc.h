/*
 * GStreamer
 * Copyright (C) 2019 Mathieu Duponchelle <mathieu@centricular.com>
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

#ifndef __GST_LINE21ENCODER_H__
#define __GST_LINE21ENCODER_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/video-anc.h>
#include "io-sim.h"

G_BEGIN_DECLS
#define GST_TYPE_LINE21ENCODER \
  (gst_line_21_encoder_get_type())
#define GST_LINE21ENCODER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_LINE21ENCODER,GstLine21Encoder))
#define GST_LINE21ENCODER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_LINE21ENCODER,GstLine21EncoderClass))
#define GST_IS_LINE21ENCODER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_LINE21ENCODER))
#define GST_IS_LINE21ENCODER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_LINE21ENCODER))

typedef struct _GstLine21Encoder GstLine21Encoder;
typedef struct _GstLine21EncoderClass GstLine21EncoderClass;

struct _GstLine21Encoder
{
  GstVideoFilter parent;

  vbi_sampling_par sp;

  GstVideoInfo info;

  gboolean remove_caption_meta;
};

struct _GstLine21EncoderClass
{
  GstVideoFilterClass parent_class;
};

GType gst_line_21_encoder_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (line21encoder);

G_END_DECLS
#endif /* __GST_LINE21ENCODER_H__ */
