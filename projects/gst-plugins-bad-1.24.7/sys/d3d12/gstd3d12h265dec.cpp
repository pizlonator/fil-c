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
 * Boston, MA 02120-1301, USA.
 */

/**
 * SECTION:element-d3d12h265dec
 * @title: d3d12h265dec
 *
 * A Direct3D12 based H.265 video decoder
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=/path/to/h265/file ! parsebin ! d3d12h265dec ! videoconvert ! autovideosink
 * ```
 *
 * Since: 1.24
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstd3d12h265dec.h"
#include <gst/dxva/gstdxvah265decoder.h>

GST_DEBUG_CATEGORY_STATIC (gst_d3d12_h265_dec_debug);
#define GST_CAT_DEFAULT gst_d3d12_h265_dec_debug

GST_D3D12_DECODER_DEFINE_TYPE (GstD3D12H265Dec, gst_d3d12_h265_dec,
    GST, D3D12_H265_DEC, GstDxvaH265Decoder);

static void
gst_d3d12_h265_dec_class_init (GstD3D12H265DecClass * klass, gpointer data)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstDxvaH265DecoderClass *dxva_class = GST_DXVA_H265_DECODER_CLASS (klass);
  GstD3D12DecoderClassData *cdata = (GstD3D12DecoderClassData *) data;

  gobject_class->finalize = gst_d3d12_h265_dec_finalize;
  gobject_class->get_property = gst_d3d12_h265_dec_get_property;

  element_class->set_context =
      GST_DEBUG_FUNCPTR (gst_d3d12_h265_dec_set_context);

  parent_class = (GstElementClass *) g_type_class_peek_parent (klass);
  gst_d3d12_decoder_class_data_fill_subclass_data (cdata, &klass->class_data);

  gst_d3d12_decoder_proxy_class_init (element_class, cdata,
      "Seungha Yang <seungha@centricualr.com>");

  decoder_class->open = GST_DEBUG_FUNCPTR (gst_d3d12_h265_dec_open);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_d3d12_h265_dec_stop);
  decoder_class->close = GST_DEBUG_FUNCPTR (gst_d3d12_h265_dec_close);
  decoder_class->negotiate = GST_DEBUG_FUNCPTR (gst_d3d12_h265_dec_negotiate);
  decoder_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d12_h265_dec_decide_allocation);
  decoder_class->sink_query = GST_DEBUG_FUNCPTR (gst_d3d12_h265_dec_sink_query);
  decoder_class->src_query = GST_DEBUG_FUNCPTR (gst_d3d12_h265_dec_src_query);
  decoder_class->sink_event = GST_DEBUG_FUNCPTR (gst_d3d12_h265_dec_sink_event);
  decoder_class->drain = GST_DEBUG_FUNCPTR (gst_d3d12_h265_dec_drain);
  decoder_class->finish = GST_DEBUG_FUNCPTR (gst_d3d12_h265_dec_finish);
  decoder_class->flush = GST_DEBUG_FUNCPTR (gst_d3d12_h265_dec_flush);

  dxva_class->configure = GST_DEBUG_FUNCPTR (gst_d3d12_h265_dec_configure);
  dxva_class->new_picture = GST_DEBUG_FUNCPTR (gst_d3d12_h265_dec_new_picture);
  dxva_class->get_picture_id =
      GST_DEBUG_FUNCPTR (gst_d3d12_h265_dec_get_picture_id);
  dxva_class->start_picture =
      GST_DEBUG_FUNCPTR (gst_d3d12_h265_dec_start_picture);
  dxva_class->end_picture = GST_DEBUG_FUNCPTR (gst_d3d12_h265_dec_end_picture);
  dxva_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_d3d12_h265_dec_output_picture);
}

static void
gst_d3d12_h265_dec_init (GstD3D12H265Dec * self)
{
  GstD3D12H265DecClass *klass = GST_D3D12_H265_DEC_GET_CLASS (self);
  GstD3D12DecoderSubClassData *cdata = &klass->class_data;

  self->decoder = gst_d3d12_decoder_new (GST_DXVA_CODEC_H265,
      cdata->adapter_luid);
}

static void
gst_d3d12_h265_dec_finalize (GObject * object)
{
  auto self = GST_D3D12_H265_DEC (object);

  gst_object_unref (self->decoder);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_d3d12_h265_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstD3D12H265DecClass *klass = GST_D3D12_H265_DEC_GET_CLASS (object);
  GstD3D12DecoderSubClassData *cdata = &klass->class_data;

  gst_d3d12_decoder_proxy_get_property (object, prop_id, value, pspec, cdata);
}

static void
gst_d3d12_h265_dec_set_context (GstElement * element, GstContext * context)
{
  auto self = GST_D3D12_H265_DEC (element);

  gst_d3d12_decoder_set_context (self->decoder, element, context);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
gst_d3d12_h265_dec_open (GstVideoDecoder * decoder)
{
  auto self = GST_D3D12_H265_DEC (decoder);

  return gst_d3d12_decoder_open (self->decoder, GST_ELEMENT (self));
}

static gboolean
gst_d3d12_h265_dec_stop (GstVideoDecoder * decoder)
{
  auto self = GST_D3D12_H265_DEC (decoder);

  gst_d3d12_decoder_stop (self->decoder);

  return GST_VIDEO_DECODER_CLASS (parent_class)->stop (decoder);
}

static gboolean
gst_d3d12_h265_dec_close (GstVideoDecoder * decoder)
{
  auto self = GST_D3D12_H265_DEC (decoder);

  return gst_d3d12_decoder_close (self->decoder);
}

static gboolean
gst_d3d12_h265_dec_negotiate (GstVideoDecoder * decoder)
{
  auto self = GST_D3D12_H265_DEC (decoder);

  if (!gst_d3d12_decoder_negotiate (self->decoder, decoder))
    return FALSE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->negotiate (decoder);
}

static gboolean
gst_d3d12_h265_dec_decide_allocation (GstVideoDecoder * decoder,
    GstQuery * query)
{
  auto self = GST_D3D12_H265_DEC (decoder);

  if (!gst_d3d12_decoder_decide_allocation (self->decoder, decoder, query)) {
    return FALSE;
  }

  return GST_VIDEO_DECODER_CLASS (parent_class)->decide_allocation
      (decoder, query);
}

static gboolean
gst_d3d12_h265_dec_sink_query (GstVideoDecoder * decoder, GstQuery * query)
{
  auto self = GST_D3D12_H265_DEC (decoder);

  if (gst_d3d12_decoder_handle_query (self->decoder, GST_ELEMENT (self), query))
    return TRUE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->sink_query (decoder, query);
}

static gboolean
gst_d3d12_h265_dec_src_query (GstVideoDecoder * decoder, GstQuery * query)
{
  auto self = GST_D3D12_H265_DEC (decoder);

  if (gst_d3d12_decoder_handle_query (self->decoder, GST_ELEMENT (self), query))
    return TRUE;

  return GST_VIDEO_DECODER_CLASS (parent_class)->src_query (decoder, query);
}

static gboolean
gst_d3d12_h265_dec_sink_event (GstVideoDecoder * decoder, GstEvent * event)
{
  auto self = GST_D3D12_H265_DEC (decoder);

  gst_d3d12_decoder_sink_event (self->decoder, event);

  return GST_VIDEO_DECODER_CLASS (parent_class)->sink_event (decoder, event);
}

static GstFlowReturn
gst_d3d12_h265_dec_drain (GstVideoDecoder * decoder)
{
  auto self = GST_D3D12_H265_DEC (decoder);

  auto ret = GST_VIDEO_DECODER_CLASS (parent_class)->drain (decoder);
  gst_d3d12_decoder_drain (self->decoder, decoder);

  return ret;
}

static GstFlowReturn
gst_d3d12_h265_dec_finish (GstVideoDecoder * decoder)
{
  auto self = GST_D3D12_H265_DEC (decoder);

  auto ret = GST_VIDEO_DECODER_CLASS (parent_class)->finish (decoder);
  gst_d3d12_decoder_drain (self->decoder, decoder);

  return ret;
}

static gboolean
gst_d3d12_h265_dec_flush (GstVideoDecoder * decoder)
{
  auto self = GST_D3D12_H265_DEC (decoder);

  auto ret = GST_VIDEO_DECODER_CLASS (parent_class)->flush (decoder);
  gst_d3d12_decoder_flush (self->decoder, decoder);

  return ret;
}

static GstFlowReturn
gst_d3d12_h265_dec_configure (GstDxvaH265Decoder * decoder,
    GstVideoCodecState * input_state, const GstVideoInfo * info,
    gint crop_x, gint crop_y, gint coded_width, gint coded_height,
    gint max_dpb_size)
{
  auto self = GST_D3D12_H265_DEC (decoder);
  auto videodec = GST_VIDEO_DECODER (decoder);

  return gst_d3d12_decoder_configure (self->decoder, videodec, input_state,
      info, crop_x, crop_y, coded_width, coded_height, max_dpb_size);
}

static GstFlowReturn
gst_d3d12_h265_dec_new_picture (GstDxvaH265Decoder * decoder,
    GstCodecPicture * picture)
{
  auto self = GST_D3D12_H265_DEC (decoder);

  return gst_d3d12_decoder_new_picture (self->decoder,
      GST_VIDEO_DECODER (decoder), picture);
}

static guint8
gst_d3d12_h265_dec_get_picture_id (GstDxvaH265Decoder * decoder,
    GstCodecPicture * picture)
{
  auto self = GST_D3D12_H265_DEC (decoder);

  return gst_d3d12_decoder_get_picture_id (self->decoder, picture);
}

static GstFlowReturn
gst_d3d12_h265_dec_start_picture (GstDxvaH265Decoder * decoder,
    GstCodecPicture * picture, guint8 * picture_id)
{
  auto self = GST_D3D12_H265_DEC (decoder);

  return gst_d3d12_decoder_start_picture (self->decoder, picture, picture_id);
}

static GstFlowReturn
gst_d3d12_h265_dec_end_picture (GstDxvaH265Decoder * decoder,
    GstCodecPicture * picture, GPtrArray * ref_pics,
    const GstDxvaDecodingArgs * args)
{
  auto self = GST_D3D12_H265_DEC (decoder);

  return gst_d3d12_decoder_end_picture (self->decoder, picture, ref_pics, args);
}

static GstFlowReturn
gst_d3d12_h265_dec_output_picture (GstDxvaH265Decoder * decoder,
    GstVideoCodecFrame * frame, GstCodecPicture * picture,
    GstVideoBufferFlags buffer_flags, gint display_width, gint display_height)
{
  auto self = GST_D3D12_H265_DEC (decoder);

  return gst_d3d12_decoder_output_picture (self->decoder,
      GST_VIDEO_DECODER (decoder), frame, picture,
      buffer_flags, display_width, display_height);
}

void
gst_d3d12_h265_dec_register (GstPlugin * plugin, GstD3D12Device * device,
    ID3D12VideoDevice * video_device, guint rank)
{
  GType type;
  gchar *type_name;
  gchar *feature_name;
  guint index = 0;
  GTypeInfo type_info = {
    sizeof (GstD3D12H265DecClass),
    nullptr,
    nullptr,
    (GClassInitFunc) gst_d3d12_h265_dec_class_init,
    nullptr,
    nullptr,
    sizeof (GstD3D12H265Dec),
    0,
    (GInstanceInitFunc) gst_d3d12_h265_dec_init,
  };

  GST_DEBUG_CATEGORY_INIT (gst_d3d12_h265_dec_debug, "d3d12h265dec", 0,
      "d3d12h265dec");

  type_info.class_data =
      gst_d3d12_decoder_check_feature_support (device, video_device,
      GST_DXVA_CODEC_H265);
  if (!type_info.class_data)
    return;

  type_name = g_strdup ("GstD3D12H265Dec");
  feature_name = g_strdup ("d3d12h265dec");

  while (g_type_from_name (type_name)) {
    index++;
    g_free (type_name);
    g_free (feature_name);
    type_name = g_strdup_printf ("GstD3D12H265Device%dDec", index);
    feature_name = g_strdup_printf ("d3d12h265device%ddec", index);
  }

  type = g_type_register_static (GST_TYPE_DXVA_H265_DECODER,
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
