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

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/codecs/gstcodecpicture.h>
#include <gst/dxva/gstdxva.h>
#include "gstd3d12.h"

G_BEGIN_DECLS

#define GST_TYPE_D3D12_DECODER (gst_d3d12_decoder_get_type())
G_DECLARE_FINAL_TYPE (GstD3D12Decoder,
    gst_d3d12_decoder, GST, D3D12_DECODER, GstObject);

typedef struct _GstD3D12DecoderClassData GstD3D12DecoderClassData;

struct GstD3D12DecoderSubClassData
{
  GstDxvaCodec codec;
  gint64 adapter_luid;
  guint device_id;
  guint vendor_id;
};

#define GST_D3D12_DECODER_DEFINE_TYPE(ModuleObjName,module_obj_name,MODULE,OBJ_NAME,ParentName) \
  static GstElementClass *parent_class = NULL; \
  typedef struct _##ModuleObjName { \
    ParentName parent; \
    GstD3D12Decoder *decoder; \
  } ModuleObjName;\
  typedef struct _##ModuleObjName##Class { \
    ParentName##Class parent_class; \
    GstD3D12DecoderSubClassData class_data; \
  } ModuleObjName##Class; \
  static inline ModuleObjName * MODULE##_##OBJ_NAME (gpointer ptr) { \
    return (ModuleObjName *) (ptr); \
  } \
  static inline ModuleObjName##Class * MODULE##_##OBJ_NAME##_GET_CLASS (gpointer ptr) { \
    return G_TYPE_INSTANCE_GET_CLASS ((ptr),G_TYPE_FROM_INSTANCE(ptr),ModuleObjName##Class); \
  } \
  static void module_obj_name##_finalize (GObject * object); \
  static void module_obj_name##_get_property (GObject * object, \
      guint prop_id, GValue * value, GParamSpec * pspec); \
  static void module_obj_name##_set_context (GstElement * element, \
      GstContext * context); \
  static gboolean module_obj_name##_open (GstVideoDecoder * decoder); \
  static gboolean module_obj_name##_stop (GstVideoDecoder * decoder); \
  static gboolean module_obj_name##_close (GstVideoDecoder * decoder); \
  static gboolean module_obj_name##_negotiate (GstVideoDecoder * decoder); \
  static gboolean module_obj_name##_decide_allocation (GstVideoDecoder * decoder, \
      GstQuery * query); \
  static gboolean module_obj_name##_sink_query (GstVideoDecoder * decoder, \
      GstQuery * query); \
  static gboolean module_obj_name##_src_query (GstVideoDecoder * decoder, \
      GstQuery * query); \
  static gboolean module_obj_name##_sink_event (GstVideoDecoder * decoder, \
      GstEvent * event); \
  static GstFlowReturn module_obj_name##_drain (GstVideoDecoder * decoder); \
  static GstFlowReturn module_obj_name##_finish (GstVideoDecoder * decoder); \
  static gboolean module_obj_name##_flush (GstVideoDecoder * decoder); \
  static GstFlowReturn module_obj_name##_configure (ParentName * decoder, \
      GstVideoCodecState * input_state, const GstVideoInfo * info, \
      gint crop_x, gint crop_y, \
      gint coded_width, gint coded_height, gint max_dpb_size); \
  static GstFlowReturn  module_obj_name##_new_picture (ParentName * decoder, \
      GstCodecPicture * picture); \
  static guint8 module_obj_name##_get_picture_id (ParentName * decoder, \
      GstCodecPicture * picture); \
  static GstFlowReturn  module_obj_name##_start_picture (ParentName * decoder, \
      GstCodecPicture * picture, guint8 * picture_id); \
  static GstFlowReturn module_obj_name##_end_picture (ParentName * decoder, \
      GstCodecPicture * picture, GPtrArray * ref_pics, \
      const GstDxvaDecodingArgs * args); \
  static GstFlowReturn module_obj_name##_output_picture (ParentName * decoder, \
      GstVideoCodecFrame * frame, GstCodecPicture * picture, \
      GstVideoBufferFlags buffer_flags, \
      gint display_width, gint display_height);

#define GST_D3D12_DECODER_DEFINE_TYPE_FULL(ModuleObjName,module_obj_name,MODULE,OBJ_NAME,ParentName) \
  GST_D3D12_DECODER_DEFINE_TYPE(ModuleObjName,module_obj_name,MODULE,OBJ_NAME,ParentName); \
  static GstFlowReturn  module_obj_name##_duplicate_picture (ParentName * decoder, \
      GstCodecPicture * src, GstCodecPicture * dst);

GstD3D12Decoder * gst_d3d12_decoder_new               (GstDxvaCodec codec,
                                                       gint64 adapter_luid);

gboolean          gst_d3d12_decoder_open              (GstD3D12Decoder * decoder,
                                                       GstElement * element);

gboolean          gst_d3d12_decoder_close             (GstD3D12Decoder * decoder);

GstFlowReturn     gst_d3d12_decoder_configure         (GstD3D12Decoder * decoder,
                                                       GstVideoDecoder * videodec,
                                                       GstVideoCodecState * input_state,
                                                       const GstVideoInfo * info,
                                                       gint crop_x,
                                                       gint crop_y,
                                                       gint coded_width,
                                                       gint coded_height,
                                                       guint dpb_size);

GstFlowReturn     gst_d3d12_decoder_drain             (GstD3D12Decoder * decoder,
                                                       GstVideoDecoder * videodec);

gboolean          gst_d3d12_decoder_flush             (GstD3D12Decoder * decoder,
                                                       GstVideoDecoder * videodec);

gboolean          gst_d3d12_decoder_stop              (GstD3D12Decoder * decoder);

GstFlowReturn     gst_d3d12_decoder_new_picture   (GstD3D12Decoder * decoder,
                                                   GstVideoDecoder * videodec,
                                                   GstCodecPicture * picture);

GstFlowReturn     gst_d3d12_decoder_new_picture_with_size (GstD3D12Decoder * decoder,
                                                           GstVideoDecoder * videodec,
                                                           GstCodecPicture * picture,
                                                           guint width,
                                                           guint height);

GstFlowReturn     gst_d3d12_decoder_duplicate_picture (GstD3D12Decoder * decoder,
                                                       GstCodecPicture * src,
                                                       GstCodecPicture * dst);

guint8            gst_d3d12_decoder_get_picture_id    (GstD3D12Decoder * decoder,
                                                       GstCodecPicture * picture);

GstFlowReturn     gst_d3d12_decoder_start_picture     (GstD3D12Decoder * decoder,
                                                       GstCodecPicture * picture,
                                                       guint8 * picture_id);

GstFlowReturn     gst_d3d12_decoder_end_picture       (GstD3D12Decoder * decoder,
                                                       GstCodecPicture * picture,
                                                       GPtrArray * ref_pics,
                                                       const GstDxvaDecodingArgs * args);

GstFlowReturn     gst_d3d12_decoder_output_picture      (GstD3D12Decoder * decoder,
                                                         GstVideoDecoder * videodec,
                                                         GstVideoCodecFrame * frame,
                                                         GstCodecPicture * picture,
                                                         GstVideoBufferFlags buffer_flags,
                                                         gint display_width,
                                                         gint display_height);

gboolean          gst_d3d12_decoder_negotiate         (GstD3D12Decoder * decoder,
                                                       GstVideoDecoder * videodec);

gboolean          gst_d3d12_decoder_decide_allocation (GstD3D12Decoder * decoder,
                                                       GstVideoDecoder * videodec,
                                                       GstQuery * query);

void              gst_d3d12_decoder_sink_event        (GstD3D12Decoder * decoder,
                                                       GstEvent * event);

void              gst_d3d12_decoder_set_context       (GstD3D12Decoder * decoder,
                                                       GstElement * element,
                                                       GstContext * context);

gboolean          gst_d3d12_decoder_handle_query      (GstD3D12Decoder * decoder,
                                                       GstElement * element,
                                                       GstQuery * query);

/* Utils for element registration */
GstD3D12DecoderClassData * gst_d3d12_decoder_check_feature_support   (GstD3D12Device * device,
                                                                      ID3D12VideoDevice * video_device,
                                                                      GstDxvaCodec codec);

void  gst_d3d12_decoder_class_data_fill_subclass_data (GstD3D12DecoderClassData * data,
                                                       GstD3D12DecoderSubClassData * subclass_data);

void  gst_d3d12_decoder_proxy_class_init              (GstElementClass * klass,
                                                       GstD3D12DecoderClassData * data,
                                                       const gchar * author);

void  gst_d3d12_decoder_proxy_get_property            (GObject * object,
                                                       guint prop_id,
                                                       GValue * value,
                                                       GParamSpec * pspec,
                                                       GstD3D12DecoderSubClassData * subclass_data);

G_END_DECLS
