/* GStreamer
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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

#ifndef __GST_NV_DECODER_H__
#define __GST_NV_DECODER_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/cuda/gstcuda.h>
#include <gst/codecs/gstcodecpicture.h>
#include "gstcuvidloader.h"
#include "gstnvdecobject.h"

G_BEGIN_DECLS

#define GST_TYPE_NV_DECODER (gst_nv_decoder_get_type())
G_DECLARE_FINAL_TYPE (GstNvDecoder,
    gst_nv_decoder, GST, NV_DECODER, GstObject);

typedef struct _GstNvDecoderClassData
{
  GstCaps *sink_caps;
  GstCaps *src_caps;
  guint cuda_device_id;
  gint64 adapter_luid;
  guint max_width;
  guint max_height;
} GstNvDecoderClassData;

GstNvDecoder * gst_nv_decoder_new (guint device_id,
                                   gint64 adapter_luid);

gboolean       gst_nv_decoder_open (GstNvDecoder * decoder,
                                    GstElement * element);

gboolean       gst_nv_decoder_close (GstNvDecoder * decoder);

gboolean       gst_nv_decoder_is_configured (GstNvDecoder * decoder);

gboolean       gst_nv_decoder_configure (GstNvDecoder * decoder,
                                         cudaVideoCodec codec,
                                         GstVideoInfo * info,
                                         gint coded_width,
                                         gint coded_height,
                                         guint coded_bitdepth,
                                         guint pool_size,
                                         gboolean alloc_aux_frame,
                                         guint num_output_surfaces,
                                         guint init_max_width,
                                         guint init_max_height);

GstFlowReturn  gst_nv_decoder_new_picture (GstNvDecoder * decoder,
                                           GstCodecPicture * picture);

gboolean       gst_nv_decoder_decode         (GstNvDecoder * decoder,
                                              CUVIDPICPARAMS * params);

GstFlowReturn  gst_nv_decoder_output_picture (GstNvDecoder * decoder,
                                              GstVideoDecoder * videodec,
                                              GstVideoCodecFrame * frame,
                                              GstCodecPicture * picture,
                                              guint buffer_flags);

void           gst_nv_decoder_set_flushing   (GstNvDecoder * decoder,
                                              gboolean flushing);

void           gst_nv_decoder_reset          (GstNvDecoder * decoder);

/* utils for class registration */
gboolean gst_nv_decoder_check_device_caps (CUcontext cuda_ctx,
                                           cudaVideoCodec codec,
                                           GstCaps **sink_template,
                                           GstCaps **src_template);

const gchar * gst_cuda_video_codec_to_string (cudaVideoCodec codec);

/* helper methods */
void     gst_nv_decoder_handle_set_context   (GstNvDecoder * decoder,
                                              GstElement * element,
                                              GstContext * context);

gboolean gst_nv_decoder_handle_query         (GstNvDecoder * decoder,
                                              GstElement * element,
                                              GstQuery * query);

gboolean gst_nv_decoder_negotiate            (GstNvDecoder * decoder,
                                              GstVideoDecoder * videodec,
                                              GstVideoCodecState * input_state);

gboolean gst_nv_decoder_decide_allocation    (GstNvDecoder * decoder,
                                              GstVideoDecoder * videodec,
                                              GstQuery * query);

guint    gst_nv_decoder_get_max_output_size  (guint coded_size,
                                              guint user_requested,
                                              guint device_max);

G_END_DECLS

#endif /* __GST_NV_DECODER_H__ */
