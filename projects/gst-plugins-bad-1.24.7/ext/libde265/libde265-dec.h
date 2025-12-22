/*
 * GStreamer HEVC/H.265 video codec.
 *
 * Copyright (c) 2014 struktur AG, Joachim Bauch <bauch@struktur.de>
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

#ifndef __GST_LIBDE265_DEC_H__
#define __GST_LIBDE265_DEC_H__

#include <gst/gst.h>
#include <gst/video/gstvideodecoder.h>

#include <libde265/de265.h>

G_BEGIN_DECLS
#define GST_TYPE_LIBDE265_DEC \
    (gst_libde265_dec_get_type())
#define GST_LIBDE265_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_LIBDE265_DEC,GstLibde265Dec))
#define GST_LIBDE265_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_LIBDE265_DEC,GstLibde265DecClass))

typedef enum
{
  GST_TYPE_LIBDE265_FORMAT_PACKETIZED,
  GST_TYPE_LIBDE265_FORMAT_BYTESTREAM
} GstLibde265DecFormat;

typedef struct _GstLibde265Dec
{
  GstVideoDecoder parent;

  /* private */
  de265_decoder_context *ctx;
  GstLibde265DecFormat format;
  int length_size;
  int max_threads;
  int buffer_full;
  void *codec_data;
  int codec_data_size;
  GstVideoCodecState *input_state;
  GstVideoCodecState *output_state;
} GstLibde265Dec;

typedef struct _GstLibde265DecClass
{
  GstVideoDecoderClass parent;
} GstLibde265DecClass;

GType gst_libde265_dec_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (libde265dec)

G_END_DECLS

#endif /* __GST_LIBDE265_DEC_H__ */
