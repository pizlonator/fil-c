/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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

#include <gst/gst.h>
#include <gst/video/video.h>
#include "gstwicimagingfactory.h"

G_BEGIN_DECLS

#define GST_TYPE_WIC_DECODER           (gst_wic_decoder_get_type())
#define GST_WIC_DECODER(obj)           (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WIC_DECODER,GstWicDecoder))
#define GST_WIC_DECODER_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_WIC_DECODER,GstWicDecoderClass))
#define GST_WIC_DECODER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_WIC_DECODER,GstWicDecoderClass))
#define GST_IS_WIC_DECODER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WIC_DECODER))
#define GST_IS_WIC_DECODER_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_WIC_DECODER))

typedef struct _GstWicDecoder GstWicDecoder;
typedef struct _GstWicDecoderClass GstWicDecoderClass;
typedef struct _GstWicDecoderPrivate GstWicDecoderPrivate;

struct _GstWicDecoder
{
  GstVideoDecoder parent;

  GstVideoCodecState *input_state;

  GstWicDecoderPrivate *priv;
};

struct _GstWicDecoderClass
{
  GstVideoDecoderClass parent_class;
  GUID codec_id;

  gboolean      (*set_format)     (GstWicDecoder * decoder,
                                   GstVideoCodecState * state);

  GstFlowReturn (*process_output) (GstWicDecoder * decoder,
                                   IWICImagingFactory * factory,
                                   IWICBitmapFrameDecode * decode_frame,
                                   GstVideoCodecFrame * frame);
};

GType     gst_wic_decoder_get_type (void);

G_END_DECLS
