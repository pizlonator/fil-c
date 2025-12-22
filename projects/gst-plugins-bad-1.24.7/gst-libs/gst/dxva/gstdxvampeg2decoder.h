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
#include <gst/codecs/gstmpeg2decoder.h>

G_BEGIN_DECLS

#define GST_TYPE_DXVA_MPEG2_DECODER            (gst_dxva_mpeg2_decoder_get_type())
#define GST_DXVA_MPEG2_DECODER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DXVA_MPEG2_DECODER,GstDxvaMpeg2Decoder))
#define GST_DXVA_MPEG2_DECODER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DXVA_MPEG2_DECODER,GstDxvaMpeg2DecoderClass))
#define GST_DXVA_MPEG2_DECODER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_DXVA_MPEG2_DECODER,GstDxvaMpeg2DecoderClass))
#define GST_IS_DXVA_MPEG2_DECODER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DXVA_MPEG2_DECODER))
#define GST_IS_DXVA_MPEG2_DECODER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DXVA_MPEG2_DECODER))

typedef struct _GstDxvaMpeg2Decoder GstDxvaMpeg2Decoder;
typedef struct _GstDxvaMpeg2DecoderClass GstDxvaMpeg2DecoderClass;
typedef struct _GstDxvaMpeg2DecoderPrivate GstDxvaMpeg2DecoderPrivate;

/**
 * GstDxvaMpeg2Decoder:
 *
 * Since: 1.24
 */
struct _GstDxvaMpeg2Decoder
{
  GstMpeg2Decoder parent;

  /*< private >*/
  GstDxvaMpeg2DecoderPrivate *priv;
};

/**
 * GstDxvaMpeg2DecoderClass:
 *
 * Since: 1.24
 */
struct _GstDxvaMpeg2DecoderClass
{
  GstMpeg2DecoderClass parent_class;

  GstFlowReturn   (*configure)          (GstDxvaMpeg2Decoder * decoder,
                                         GstVideoCodecState * input_state,
                                         const GstVideoInfo * info,
                                         gint crop_x,
                                         gint crop_y,
                                         gint coded_width,
                                         gint coded_height,
                                         gint max_dpb_size);

  GstFlowReturn   (*new_picture)        (GstDxvaMpeg2Decoder * decoder,
                                         GstCodecPicture * picture);

  GstFlowReturn   (*duplicate_picture)  (GstDxvaMpeg2Decoder * decoder,
                                         GstCodecPicture * src,
                                         GstCodecPicture * dst);

  guint8          (*get_picture_id)     (GstDxvaMpeg2Decoder * decoder,
                                         GstCodecPicture * picture);

  GstFlowReturn   (*start_picture)      (GstDxvaMpeg2Decoder * decoder,
                                         GstCodecPicture * picture,
                                         guint8 * picture_id);

  GstFlowReturn   (*end_picture)        (GstDxvaMpeg2Decoder * decoder,
                                         GstCodecPicture * picture,
                                         GPtrArray * ref_pics,
                                         const GstDxvaDecodingArgs * args);

  GstFlowReturn   (*output_picture)     (GstDxvaMpeg2Decoder * decoder,
                                         GstVideoCodecFrame * frame,
                                         GstCodecPicture * picture,
                                         GstVideoBufferFlags buffer_flags,
                                         gint display_width,
                                         gint display_height);
};

GST_DXVA_API
GType gst_dxva_mpeg2_decoder_get_type (void);

GST_DXVA_API
void  gst_dxva_mpeg2_decoder_disable_postproc (GstDxvaMpeg2Decoder * decoder);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstDxvaMpeg2Decoder, gst_object_unref)

G_END_DECLS

