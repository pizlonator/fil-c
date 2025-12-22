/* GStreamer
 * Copyright (C) <2017> Sean DuBois <sean@siobud.com>
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


#ifndef __GST_AV1_DEC_H__
#define __GST_AV1_DEC_H__


#include <gst/gst.h>
#include <gst/video/gstvideodecoder.h>

#include <aom/aomdx.h>
#include <aom/aom_decoder.h>

G_BEGIN_DECLS
#define GST_TYPE_AV1_DEC \
  (gst_av1_dec_get_type())
#define GST_AV1_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AV1_DEC,GstAV1Dec))
#define GST_AV1_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AV1_DEC,GstAV1DecClass))
#define GST_IS_AV1_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AV1_DEC))
#define GST_IS_AV1_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AV1_DEC))
#define GST_AV1_DEC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_AV1_DEC, GstAV1DecClass))
#define GST_AV1_DEC_CAST(obj) \
  ((GstAV1Dec *) (obj))

typedef struct _GstAV1Dec GstAV1Dec;
typedef struct _GstAV1DecClass GstAV1DecClass;

struct _GstAV1Dec
{
  GstVideoDecoder base_video_decoder;

  gboolean decoder_inited;

  aom_codec_ctx_t decoder;

  GstVideoCodecState *input_state;
  GstVideoCodecState *output_state;
};

struct _GstAV1DecClass
{
  GstVideoDecoderClass parent_class;
  /*supported aom algo*/
  aom_codec_iface_t* codec_algo;
};

GType gst_av1_dec_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (av1dec);

G_END_DECLS
#endif /* __GST_AV1_DEC_H__ */
