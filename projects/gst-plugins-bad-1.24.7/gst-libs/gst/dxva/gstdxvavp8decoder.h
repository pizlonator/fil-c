/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include <gst/dxva/dxva-prelude.h>
#include <gst/dxva/gstdxvatypes.h>
#include <gst/codecs/gstvp8decoder.h>

G_BEGIN_DECLS

#define GST_TYPE_DXVA_VP8_DECODER            (gst_dxva_vp8_decoder_get_type())
#define GST_DXVA_VP8_DECODER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DXVA_VP8_DECODER,GstDxvaVp8Decoder))
#define GST_DXVA_VP8_DECODER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DXVA_VP8_DECODER,GstDxvaVp8DecoderClass))
#define GST_DXVA_VP8_DECODER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_DXVA_VP8_DECODER,GstDxvaVp8DecoderClass))
#define GST_IS_DXVA_VP8_DECODER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DXVA_VP8_DECODER))
#define GST_IS_DXVA_VP8_DECODER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DXVA_VP8_DECODER))

typedef struct _GstDxvaVp8Decoder GstDxvaVp8Decoder;
typedef struct _GstDxvaVp8DecoderClass GstDxvaVp8DecoderClass;
typedef struct _GstDxvaVp8DecoderPrivate GstDxvaVp8DecoderPrivate;

/**
 * GstDxvaVp8Decoder:
 *
 * Since: 1.24
 */
struct _GstDxvaVp8Decoder
{
  GstVp8Decoder parent;

  /*< private >*/
  GstDxvaVp8DecoderPrivate *priv;
};

/**
 * GstDxvaVp8DecoderClass:
 *
 * Since: 1.24
 */
struct _GstDxvaVp8DecoderClass
{
  GstVp8DecoderClass parent_class;

  GstFlowReturn   (*configure)          (GstDxvaVp8Decoder * decoder,
                                         GstVideoCodecState * input_state,
                                         const GstVideoInfo * info,
                                         gint crop_x,
                                         gint crop_y,
                                         gint coded_width,
                                         gint coded_height,
                                         gint max_dpb_size);

  GstFlowReturn   (*new_picture)        (GstDxvaVp8Decoder * decoder,
                                         GstCodecPicture * picture);

  guint8          (*get_picture_id)     (GstDxvaVp8Decoder * decoder,
                                         GstCodecPicture * picture);

  GstFlowReturn   (*start_picture)      (GstDxvaVp8Decoder * decoder,
                                         GstCodecPicture * picture,
                                         guint8 * picture_id);

  GstFlowReturn   (*end_picture)        (GstDxvaVp8Decoder * decoder,
                                         GstCodecPicture * picture,
                                         GPtrArray * ref_pics,
                                         const GstDxvaDecodingArgs * args);

  GstFlowReturn   (*output_picture)     (GstDxvaVp8Decoder * decoder,
                                         GstVideoCodecFrame * frame,
                                         GstCodecPicture * picture,
                                         GstVideoBufferFlags buffer_flags,
                                         gint display_width,
                                         gint display_height);
};

GST_DXVA_API
GType gst_dxva_vp8_decoder_get_type (void);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstDxvaVp8Decoder, gst_object_unref)

G_END_DECLS

