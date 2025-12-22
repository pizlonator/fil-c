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

/**
 * SECTION:element-d3d11h264dec
 * @title: d3d11h264dec
 *
 * A Direct3D11/DXVA based H.264 video decoder
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=/path/to/h264/file ! parsebin ! d3d11h264dec ! d3d11videosink
 * ```
 *
 * Since: 1.18
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstd3d11h264dec.h"
#include <gst/dxva/gstdxvah264decoder.h>

GST_DEBUG_CATEGORY_EXTERN (gst_d3d11_h264_dec_debug);
#define GST_CAT_DEFAULT gst_d3d11_h264_dec_debug

GST_D3D11_DECODER_DEFINE_TYPE_FULL (GstD3D11H264Dec, gst_d3d11_h264_dec,
    GST, D3D11_H264_DEC, GstDxvaH264Decoder);

static void
gst_d3d11_h264_dec_class_init (GstD3D11H264DecClass * klass, gpointer data)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstDxvaH264DecoderClass *dxva_class = GST_DXVA_H264_DECODER_CLASS (klass);
  GstD3D11DecoderClassData *cdata = (GstD3D11DecoderClassData *) data;

  gobject_class->get_property = gst_d3d11_h264_dec_get_property;

  element_class->set_context =
      GST_DEBUG_FUNCPTR (gst_d3d11_h264_dec_set_context);

  parent_class = (GstElementClass *) g_type_class_peek_parent (klass);
  gst_d3d11_decoder_class_data_fill_subclass_data (cdata, &klass->class_data);

  /**
   * GstD3D11H264Dec:adapter-luid:
   *
   * DXGI Adapter LUID for this element
   *
   * Since: 1.20
   */
  gst_d3d11_decoder_proxy_class_init (element_class, cdata,
      "Seungha Yang <seungha.yang@navercorp.com>");

  decoder_class->open = GST_DEBUG_FUNCPTR (gst_d3d11_h264_dec_open);
  decoder_class->close = GST_DEBUG_FUNCPTR (gst_d3d11_h264_dec_close);
  decoder_class->negotiate = GST_DEBUG_FUNCPTR (gst_d3d11_h264_dec_negotiate);
  decoder_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d11_h264_dec_decide_allocation);
  decoder_class->sink_query = GST_DEBUG_FUNCPTR (gst_d3d11_h264_dec_sink_query);
  decoder_class->src_query = GST_DEBUG_FUNCPTR (gst_d3d11_h264_dec_src_query);
  decoder_class->sink_event = GST_DEBUG_FUNCPTR (gst_d3d11_h264_dec_sink_event);

  dxva_class->configure = GST_DEBUG_FUNCPTR (gst_d3d11_h264_dec_configure);
  dxva_class->new_picture = GST_DEBUG_FUNCPTR (gst_d3d11_h264_dec_new_picture);
  dxva_class->duplicate_picture =
      GST_DEBUG_FUNCPTR (gst_d3d11_h264_dec_duplicate_picture);
  dxva_class->get_picture_id =
      GST_DEBUG_FUNCPTR (gst_d3d11_h264_dec_get_picture_id);
  dxva_class->start_picture =
      GST_DEBUG_FUNCPTR (gst_d3d11_h264_dec_start_picture);
  dxva_class->end_picture = GST_DEBUG_FUNCPTR (gst_d3d11_h264_dec_end_picture);
  dxva_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_d3d11_h264_dec_output_picture);
}

static void
gst_d3d11_h264_dec_init (GstD3D11H264Dec * self)
{
}

static void
gst_d3d11_h264_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstD3D11H264DecClass *klass = GST_D3D11_H264_DEC_GET_CLASS (object);
  GstD3D11DecoderSubClassData *cdata = &klass->class_data;

  gst_d3d11_decoder_proxy_get_property (object, prop_id, value, pspec, cdata);
}

static void
gst_d3d11_h264_dec_set_context (GstElement * element, GstContext * context)
{
  GstD3D11H264Dec *self = GST_D3D11_H264_DEC (element);
  GstD3D11H264DecClass *klass = GST_D3D11_H264_DEC_GET_CLASS (self);
  GstD3D11DecoderSubClassData *cdata = &klass->class_data;

  gst_d3d11_handle_set_context_for_adapter_luid (element,
      context, cdata->adapter_luid, &self->device);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
gst_d3d11_h264_dec_open (GstVideoDecoder * decoder)
{
  GstD3D11H264Dec *self = GST_D3D11_H264_DEC (decoder);
  GstD3D11H264DecClass *klass = GST_D3D11_H264_DEC_GET_CLASS (self);
  GstD3D11DecoderSubClassData *cdata = &klass->class_data;

  return gst_d3d11_decoder_proxy_open (decoder,
      cdata, &self->device, &self->decoder);
}

static gboolean
gst_d3d11_h264_dec_close (GstVideoDecoder * decoder)
{
  GstD3D11H264Dec *self = GST_D3D11_H264_DEC (decoder);

  gst_clear_object (&self->decoder);
  gst_clear_object (&self->device);

  return TRUE;
}

static gboolean
gst_d3d11_h264_dec_negotiate (GstVideoDecoder * decoder)
{
  GstD3D11H264Dec *self = GST_D3D11_H264_DEC (decoder);

  if (!gst_d3d11_decoder_negotiate (self->decoder, decoder))
    return FALSE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->negotiate (decoder);
}

static gboolean
gst_d3d11_h264_dec_decide_allocation (GstVideoDecoder * decoder,
    GstQuery * query)
{
  GstD3D11H264Dec *self = GST_D3D11_H264_DEC (decoder);

  if (!gst_d3d11_decoder_decide_allocation (self->decoder, decoder, query))
    return FALSE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->decide_allocation
      (decoder, query);
}

static gboolean
gst_d3d11_h264_dec_sink_query (GstVideoDecoder * decoder, GstQuery * query)
{
  GstD3D11H264Dec *self = GST_D3D11_H264_DEC (decoder);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      if (gst_d3d11_handle_context_query (GST_ELEMENT (decoder),
              query, self->device)) {
        return TRUE;
      }
      break;
    default:
      break;
  }

  return GST_VIDEO_DECODER_CLASS (parent_class)->sink_query (decoder, query);
}

static gboolean
gst_d3d11_h264_dec_src_query (GstVideoDecoder * decoder, GstQuery * query)
{
  GstD3D11H264Dec *self = GST_D3D11_H264_DEC (decoder);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      if (gst_d3d11_handle_context_query (GST_ELEMENT (decoder),
              query, self->device)) {
        return TRUE;
      }
      break;
    default:
      break;
  }

  return GST_VIDEO_DECODER_CLASS (parent_class)->src_query (decoder, query);
}

static gboolean
gst_d3d11_h264_dec_sink_event (GstVideoDecoder * decoder, GstEvent * event)
{
  GstD3D11H264Dec *self = GST_D3D11_H264_DEC (decoder);

  if (self->decoder)
    gst_d3d11_decoder_sink_event (self->decoder, event);

  return GST_VIDEO_DECODER_CLASS (parent_class)->sink_event (decoder, event);
}

static GstFlowReturn
gst_d3d11_h264_dec_configure (GstDxvaH264Decoder * decoder,
    GstVideoCodecState * input_state, const GstVideoInfo * info,
    gint crop_x, gint crop_y, gint coded_width, gint coded_height,
    gint max_dpb_size)
{
  GstD3D11H264Dec *self = GST_D3D11_H264_DEC (decoder);

  return gst_d3d11_decoder_configure (self->decoder, input_state,
      info, crop_x, crop_y, coded_width, coded_height, max_dpb_size);
}

static GstFlowReturn
gst_d3d11_h264_dec_new_picture (GstDxvaH264Decoder * decoder,
    GstCodecPicture * picture)
{
  GstD3D11H264Dec *self = GST_D3D11_H264_DEC (decoder);

  return gst_d3d11_decoder_new_picture (self->decoder,
      GST_VIDEO_DECODER (decoder), picture);
}

static GstFlowReturn
gst_d3d11_h264_dec_duplicate_picture (GstDxvaH264Decoder * decoder,
    GstCodecPicture * src, GstCodecPicture * dst)
{
  GstD3D11H264Dec *self = GST_D3D11_H264_DEC (decoder);

  return gst_d3d11_decoder_duplicate_picture (self->decoder, src, dst);
}

static guint8
gst_d3d11_h264_dec_get_picture_id (GstDxvaH264Decoder * decoder,
    GstCodecPicture * picture)
{
  GstD3D11H264Dec *self = GST_D3D11_H264_DEC (decoder);

  return gst_d3d11_decoder_get_picture_id (self->decoder, picture);
}

static GstFlowReturn
gst_d3d11_h264_dec_start_picture (GstDxvaH264Decoder * decoder,
    GstCodecPicture * picture, guint8 * picture_id)
{
  GstD3D11H264Dec *self = GST_D3D11_H264_DEC (decoder);

  return gst_d3d11_decoder_start_picture (self->decoder, picture, picture_id);
}

static GstFlowReturn
gst_d3d11_h264_dec_end_picture (GstDxvaH264Decoder * decoder,
    GstCodecPicture * picture, GPtrArray * ref_pics,
    const GstDxvaDecodingArgs * args)
{
  GstD3D11H264Dec *self = GST_D3D11_H264_DEC (decoder);

  return gst_d3d11_decoder_end_picture (self->decoder, picture, args);
}

static GstFlowReturn
gst_d3d11_h264_dec_output_picture (GstDxvaH264Decoder * decoder,
    GstVideoCodecFrame * frame, GstCodecPicture * picture,
    GstVideoBufferFlags buffer_flags, gint display_width, gint display_height)
{
  GstD3D11H264Dec *self = GST_D3D11_H264_DEC (decoder);

  return gst_d3d11_decoder_output_picture (self->decoder,
      GST_VIDEO_DECODER (decoder), frame, picture,
      buffer_flags, display_width, display_height);
}

void
gst_d3d11_h264_dec_register (GstPlugin * plugin, GstD3D11Device * device,
    guint rank, gboolean legacy)
{
  GType type;
  gchar *type_name;
  gchar *feature_name;
  guint index = 0;
  guint i;
  gboolean ret;
  GTypeInfo type_info = {
    sizeof (GstD3D11H264DecClass),
    nullptr,
    nullptr,
    (GClassInitFunc) gst_d3d11_h264_dec_class_init,
    nullptr,
    nullptr,
    sizeof (GstD3D11H264Dec),
    0,
    (GInstanceInitFunc) gst_d3d11_h264_dec_init,
  };
  const GUID *supported_profile = nullptr;
  GstCaps *sink_caps = nullptr;
  GstCaps *src_caps = nullptr;
  guint max_width = 0;
  guint max_height = 0;
  guint resolution;

  ret = gst_d3d11_decoder_get_supported_decoder_profile (device,
      GST_DXVA_CODEC_H264, GST_VIDEO_FORMAT_NV12, &supported_profile);

  if (!ret) {
    GST_WARNING_OBJECT (device, "decoder profile unavailable");
    return;
  }

  ret = gst_d3d11_decoder_supports_format (device, supported_profile,
      DXGI_FORMAT_NV12);
  if (!ret) {
    GST_FIXME_OBJECT (device, "device does not support NV12 format");
    return;
  }

  /* we will not check the maximum resolution for legacy devices.
   * it might cause crash */
  if (legacy) {
    max_width = gst_dxva_resolutions[0].width;
    max_height = gst_dxva_resolutions[0].height;
  } else {
    for (i = 0; i < G_N_ELEMENTS (gst_dxva_resolutions); i++) {
      if (gst_d3d11_decoder_supports_resolution (device, supported_profile,
              DXGI_FORMAT_NV12, gst_dxva_resolutions[i].width,
              gst_dxva_resolutions[i].height)) {
        max_width = gst_dxva_resolutions[i].width;
        max_height = gst_dxva_resolutions[i].height;

        GST_DEBUG_OBJECT (device,
            "device support resolution %dx%d", max_width, max_height);
      } else {
        break;
      }
    }
  }

  if (max_width == 0 || max_height == 0) {
    GST_WARNING_OBJECT (device, "Couldn't query supported resolution");
    return;
  }

  sink_caps = gst_caps_from_string ("video/x-h264, "
      "stream-format= (string) { avc, avc3, byte-stream }, "
      "alignment= (string) au, "
      "profile = (string) { high, progressive-high, constrained-high, main, constrained-baseline, baseline }");
  src_caps = gst_caps_from_string ("video/x-raw("
      GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY "), format = (string) NV12; "
      "video/x-raw, format = (string) NV12");

  /* To cover both landscape and portrait, select max value */
  resolution = MAX (max_width, max_height);

  type_info.class_data =
      gst_d3d11_decoder_class_data_new (device, GST_DXVA_CODEC_H264,
      sink_caps, src_caps, resolution);

  type_name = g_strdup ("GstD3D11H264Dec");
  feature_name = g_strdup ("d3d11h264dec");

  while (g_type_from_name (type_name)) {
    index++;
    g_free (type_name);
    g_free (feature_name);
    type_name = g_strdup_printf ("GstD3D11H264Device%dDec", index);
    feature_name = g_strdup_printf ("d3d11h264device%ddec", index);
  }

  type = g_type_register_static (GST_TYPE_DXVA_H264_DECODER,
      type_name, &type_info, (GTypeFlags) 0);

  /* make lower rank than default device */
  if (rank > 0 && index != 0)
    rank--;

  if (index != 0)
    gst_element_type_set_skip_documentation (type);

  if (!gst_element_register (plugin, feature_name, rank, type))
    GST_WARNING ("Failed to register plugin '%s'", type_name);

  g_free (type_name);
  g_free (feature_name);
}
