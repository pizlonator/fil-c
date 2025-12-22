/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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

#ifndef __GST_D3D11_DECODER_H__
#define __GST_D3D11_DECODER_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/d3d11/gstd3d11.h>
#include <gst/codecs/gstcodecpicture.h>
#include <gst/dxva/gstdxva.h>

G_BEGIN_DECLS

#define GST_TYPE_D3D11_DECODER (gst_d3d11_decoder_get_type())
G_DECLARE_FINAL_TYPE (GstD3D11Decoder,
    gst_d3d11_decoder, GST, D3D11_DECODER, GstObject);

typedef struct _GstD3D11DecoderClassData GstD3D11DecoderClassData;

struct GstD3D11DecoderSubClassData
{
  GstDxvaCodec codec;
  gint64 adapter_luid;
  guint device_id;
  guint vendor_id;
};

#define GST_D3D11_DECODER_DEFINE_TYPE(ModuleObjName,module_obj_name,MODULE,OBJ_NAME,ParentName) \
  static GstElementClass *parent_class = NULL; \
  typedef struct _##ModuleObjName { \
    ParentName parent; \
    GstD3D11Device *device; \
    GstD3D11Decoder *decoder; \
  } ModuleObjName;\
  typedef struct _##ModuleObjName##Class { \
    ParentName##Class parent_class; \
    GstD3D11DecoderSubClassData class_data; \
  } ModuleObjName##Class; \
  static inline ModuleObjName * MODULE##_##OBJ_NAME (gpointer ptr) { \
    return (ModuleObjName *) (ptr); \
  } \
  static inline ModuleObjName##Class * MODULE##_##OBJ_NAME##_GET_CLASS (gpointer ptr) { \
    return G_TYPE_INSTANCE_GET_CLASS ((ptr),G_TYPE_FROM_INSTANCE(ptr),ModuleObjName##Class); \
  } \
  static void module_obj_name##_get_property (GObject * object, \
      guint prop_id, GValue * value, GParamSpec * pspec); \
  static void module_obj_name##_set_context (GstElement * element, \
      GstContext * context); \
  static gboolean module_obj_name##_open (GstVideoDecoder * decoder); \
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

#define GST_D3D11_DECODER_DEFINE_TYPE_FULL(ModuleObjName,module_obj_name,MODULE,OBJ_NAME,ParentName) \
  GST_D3D11_DECODER_DEFINE_TYPE(ModuleObjName,module_obj_name,MODULE,OBJ_NAME,ParentName); \
  static GstFlowReturn  module_obj_name##_duplicate_picture (ParentName * decoder, \
      GstCodecPicture * src, GstCodecPicture * dst);

GstD3D11Decoder * gst_d3d11_decoder_new (GstD3D11Device * device,
                                         GstDxvaCodec codec);

GstFlowReturn     gst_d3d11_decoder_configure     (GstD3D11Decoder * decoder,
                                                   GstVideoCodecState * input_state,
                                                   const GstVideoInfo * out_info,
                                                   gint offset_x,
                                                   gint offset_y,
                                                   gint coded_width,
                                                   gint coded_height,
                                                   guint dpb_size);

GstFlowReturn     gst_d3d11_decoder_new_picture   (GstD3D11Decoder * decoder,
                                                   GstVideoDecoder * videodec,
                                                   GstCodecPicture * picture);

GstFlowReturn     gst_d3d11_decoder_duplicate_picture (GstD3D11Decoder * decoder,
                                                       GstCodecPicture * src,
                                                       GstCodecPicture * dst);

guint8            gst_d3d11_decoder_get_picture_id    (GstD3D11Decoder * decoder,
                                                       GstCodecPicture * picture);

GstFlowReturn     gst_d3d11_decoder_start_picture     (GstD3D11Decoder * decoder,
                                                       GstCodecPicture * picture,
                                                       guint8 * picture_id);

GstFlowReturn     gst_d3d11_decoder_end_picture       (GstD3D11Decoder * decoder,
                                                       GstCodecPicture * picture,
                                                       const GstDxvaDecodingArgs * args);

GstFlowReturn     gst_d3d11_decoder_output_picture      (GstD3D11Decoder * decoder,
                                                         GstVideoDecoder * videodec,
                                                         GstVideoCodecFrame * frame,
                                                         GstCodecPicture * picture,
                                                         GstVideoBufferFlags buffer_flags,
                                                         gint display_width,
                                                         gint display_height);

gboolean          gst_d3d11_decoder_negotiate           (GstD3D11Decoder * decoder,
                                                         GstVideoDecoder * videodec);

gboolean          gst_d3d11_decoder_decide_allocation   (GstD3D11Decoder * decoder,
                                                         GstVideoDecoder * videodec,
                                                         GstQuery * query);

void              gst_d3d11_decoder_sink_event          (GstD3D11Decoder * decoder,
                                                         GstEvent * event);

gboolean          gst_d3d11_decoder_util_is_legacy_device (GstD3D11Device * device);

gboolean          gst_d3d11_decoder_get_supported_decoder_profile (GstD3D11Device * device,
                                                                   GstDxvaCodec codec,
                                                                   GstVideoFormat format,
                                                                   const GUID ** selected_profile);

gboolean          gst_d3d11_decoder_supports_format (GstD3D11Device * device,
                                                     const GUID * decoder_profile,
                                                     DXGI_FORMAT format);

gboolean          gst_d3d11_decoder_supports_resolution (GstD3D11Device * device,
                                                         const GUID * decoder_profile,
                                                         DXGI_FORMAT format,
                                                         guint width,
                                                         guint height);

GstD3D11DecoderClassData *  gst_d3d11_decoder_class_data_new  (GstD3D11Device * device,
                                                               GstDxvaCodec codec,
                                                               GstCaps * sink_caps,
                                                               GstCaps * src_caps,
                                                               guint max_resolution);

void  gst_d3d11_decoder_class_data_fill_subclass_data (GstD3D11DecoderClassData * data,
                                                       GstD3D11DecoderSubClassData * subclass_data);

void  gst_d3d11_decoder_proxy_class_init              (GstElementClass * klass,
                                                       GstD3D11DecoderClassData * data,
                                                       const gchar * author);

void  gst_d3d11_decoder_proxy_get_property            (GObject * object,
                                                       guint prop_id,
                                                       GValue * value,
                                                       GParamSpec * pspec,
                                                       GstD3D11DecoderSubClassData * subclass_data);

gboolean gst_d3d11_decoder_proxy_open                 (GstVideoDecoder * videodec,
                                                       GstD3D11DecoderSubClassData * subclass_data,
                                                       GstD3D11Device ** device,
                                                       GstD3D11Decoder ** decoder);

G_END_DECLS

#endif /* __GST_D3D11_DECODER_H__ */
