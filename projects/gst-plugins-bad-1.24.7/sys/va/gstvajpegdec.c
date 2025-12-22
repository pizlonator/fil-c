/* GStreamer
 * Copyright (C) 2022 Igalia, S.L.
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
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
 * License along with this library; if not, write to the0
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:element-vajpegdec
 * @title: vajpegdec
 * @short_description: A VA-API based JPEG video decoder
 *
 * vajpegdec decodes JPEG images to VA surfaces using the installed
 * and chosen [VA-API](https://01.org/linuxmedia/vaapi) driver.
 *
 * The decoding surfaces can be mapped onto main memory as video
 * frames.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=sample.mjpg ! parsebin ! vajpegdec ! autovideosink
 * ```
 *
 * Since: 1.22
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvajpegdec.h"

#include <gst/va/gstvavideoformat.h>

#include "gstvacaps.h"
#include "gstvabasedec.h"

GST_DEBUG_CATEGORY_STATIC (gst_va_jpegdec_debug);
#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT gst_va_jpegdec_debug
#else
#define GST_CAT_DEFAULT NULL
#endif

#define GST_VA_JPEG_DEC(obj)           ((GstVaJpegDec *) obj)
#define GST_VA_JPEG_DEC_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_FROM_INSTANCE (obj), GstVaJPEGDecClass))
#define GST_VA_JPEG_DEC_CLASS(klass)   ((GstVaJpegDecClass *) klass)

typedef struct _GstVaJpegDec GstVaJpegDec;
typedef struct _GstVaJpegDecClass GstVaJpegDecClass;

struct _GstVaJpegDecClass
{
  GstVaBaseDecClass parent_class;
};

struct _GstVaJpegDec
{
  GstVaBaseDec parent;

  GstVaDecodePicture *pic;
};

static GstElementClass *parent_class = NULL;

/* *INDENT-OFF* */
static const gchar *src_caps_str =
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_VA,
        "{ NV12 }") " ;"
    GST_VIDEO_CAPS_MAKE ("{ NV12 }");
/* *INDENT-ON* */

static const gchar *sink_caps_str = "image/jpeg";

static VAProfile
_get_profile (GstJpegMarker marker)
{
  switch (marker) {
    case GST_JPEG_MARKER_SOF0:
      return VAProfileJPEGBaseline;
    default:
      break;
  };

  return VAProfileNone;
}

/* taken from MediaSDK */
#define RT_FORMAT_RGB (VA_RT_FORMAT_RGB16 | VA_RT_FORMAT_RGB32)

/* *INDENT-OFF* */
static const struct sampling_rtformat {
  const gchar *sampling;
  guint32 rt_format;
} sampling_rtformat_map[] = {
  { "RGB", RT_FORMAT_RGB },
  { "YCbCr-4:4:4", VA_RT_FORMAT_YUV444 },
  { "YCbCr-4:2:2", VA_RT_FORMAT_YUV422 },
  { "YCbCr-4:2:0", VA_RT_FORMAT_YUV420 },
  { "GRAYSCALE", VA_RT_FORMAT_YUV400 },
  { "YCbCr-4:1:1", VA_RT_FORMAT_YUV411 },
};
/* *INDENT-ON* */

static guint32
_get_rt_format (GstCaps * caps)
{
  GstStructure *structure;
  const gchar *sampling;
  guint i;

  structure = gst_caps_get_structure (caps, 0);
  sampling = gst_structure_get_string (structure, "sampling");

  for (i = 0; i < G_N_ELEMENTS (sampling_rtformat_map); i++) {
    if (g_strcmp0 (sampling, sampling_rtformat_map[i].sampling) == 0)
      return sampling_rtformat_map[i].rt_format;
  }

  return 0;
}

static GstFlowReturn
gst_va_jpeg_dec_new_picture (GstJpegDecoder * decoder,
    GstVideoCodecFrame * frame, GstJpegMarker marker,
    GstJpegFrameHdr * frame_hdr)
{
  GstVaJpegDec *self = GST_VA_JPEG_DEC (decoder);
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVideoInfo *info = &base->output_info;
  GstFlowReturn ret;
  VAProfile profile;
  VAPictureParameterBufferJPEGBaseline pic_param;
  guint32 i, rt_format;

  GST_LOG_OBJECT (self, "new picture");

  g_clear_pointer (&self->pic, gst_va_decode_picture_free);

  profile = _get_profile (marker);
  if (profile == VAProfileNone)
    return GST_FLOW_NOT_NEGOTIATED;

  /* use caps to avoid re-parsing app14 */
  rt_format = _get_rt_format (decoder->input_state->caps);
  if (rt_format == 0)
    return GST_FLOW_NOT_NEGOTIATED;

  if (!gst_va_decoder_config_is_equal (base->decoder, profile, rt_format,
          frame_hdr->width, frame_hdr->height)) {
    base->profile = profile;
    base->rt_format = rt_format;
    GST_VIDEO_INFO_WIDTH (info) = base->width = frame_hdr->width;
    GST_VIDEO_INFO_HEIGHT (info) = base->height = frame_hdr->height;

    base->need_negotiation = TRUE;
    GST_INFO_OBJECT (self, "Format changed to %s [%x] (%dx%d)",
        gst_va_profile_name (profile), rt_format, base->width, base->height);
  }

  g_clear_pointer (&base->input_state, gst_video_codec_state_unref);
  base->input_state = gst_video_codec_state_ref (decoder->input_state);

  ret = gst_va_base_dec_prepare_output_frame (base, frame);
  if (ret != GST_FLOW_OK) {
    GST_ERROR_OBJECT (self, "Failed to allocate output buffer: %s",
        gst_flow_get_name (ret));
    return ret;
  }

  self->pic = gst_va_decode_picture_new (base->decoder, frame->output_buffer);

  /* *INDENT-OFF* */
  pic_param = (VAPictureParameterBufferJPEGBaseline) {
    .picture_width = frame_hdr->width,
    .picture_height = frame_hdr->height,
    /* .components */
    .num_components = frame_hdr->num_components,
    .color_space = (rt_format == RT_FORMAT_RGB) ? 1 : 0, /* TODO: BGR */
    .rotation = VA_ROTATION_NONE,
  };
  /* *INDENT-ON* */

  for (i = 0; i < frame_hdr->num_components; i++) {
    pic_param.components[i].component_id = frame_hdr->components[i].identifier;
    pic_param.components[i].h_sampling_factor =
        frame_hdr->components[i].horizontal_factor;
    pic_param.components[i].v_sampling_factor =
        frame_hdr->components[i].vertical_factor;
    pic_param.components[i].quantiser_table_selector =
        frame_hdr->components[i].quant_table_selector;
  }

  if (!gst_va_decoder_add_param_buffer (base->decoder, self->pic,
          VAPictureParameterBufferType, &pic_param, sizeof (pic_param)))
    return GST_FLOW_ERROR;

  return ret;
}

static GstFlowReturn
gst_va_jpeg_dec_decode_scan (GstJpegDecoder * decoder,
    GstJpegDecoderScan * scan, const guint8 * buffer, guint32 size)
{
  GstVaJpegDec *self = GST_VA_JPEG_DEC (decoder);
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  VAHuffmanTableBufferJPEGBaseline huff = { 0, };
  VAIQMatrixBufferJPEGBaseline quant = { 0, };
  VASliceParameterBufferJPEGBaseline slice_param;
  guint i, j;

  GST_LOG_OBJECT (self, "decoding slice");

  for (i = 0; i < G_N_ELEMENTS (quant.load_quantiser_table); i++) {
    quant.load_quantiser_table[i] =
        scan->quantization_tables->quant_tables[i].valid;
    if (!scan->quantization_tables->quant_tables[i].valid)
      continue;
    for (j = 0; j < GST_JPEG_MAX_QUANT_ELEMENTS; j++) {
      quant.quantiser_table[i][j] =
          scan->quantization_tables->quant_tables[i].quant_table[j];
    }

    /* invalidate table */
    scan->quantization_tables->quant_tables[i].valid = FALSE;
  }

  if (!gst_va_decoder_add_param_buffer (base->decoder, self->pic,
          VAIQMatrixBufferType, &quant, sizeof (quant)))
    return GST_FLOW_ERROR;

  for (i = 0; i < G_N_ELEMENTS (huff.huffman_table); i++) {
    huff.load_huffman_table[i] = scan->huffman_tables->dc_tables[i].valid
        && scan->huffman_tables->ac_tables[i].valid;

    if (!huff.load_huffman_table[i])
      continue;

    memcpy (huff.huffman_table[i].num_dc_codes,
        scan->huffman_tables->dc_tables[i].huf_bits,
        sizeof (huff.huffman_table[i].num_dc_codes));

    memcpy (huff.huffman_table[i].dc_values,
        scan->huffman_tables->dc_tables[i].huf_values,
        sizeof (huff.huffman_table[i].dc_values));

    memcpy (huff.huffman_table[i].num_ac_codes,
        scan->huffman_tables->ac_tables[i].huf_bits,
        sizeof (huff.huffman_table[i].num_ac_codes));

    memcpy (huff.huffman_table[i].ac_values,
        scan->huffman_tables->ac_tables[i].huf_values,
        sizeof (huff.huffman_table[i].ac_values));
  }


  /* invalidate table */
  for (i = 0; i < G_N_ELEMENTS (scan->huffman_tables->dc_tables); i++)
    scan->huffman_tables->dc_tables[i].valid = FALSE;
  for (i = 0; i < G_N_ELEMENTS (scan->huffman_tables->ac_tables); i++)
    scan->huffman_tables->ac_tables[i].valid = FALSE;

  if (!gst_va_decoder_add_param_buffer (base->decoder, self->pic,
          VAHuffmanTableBufferType, &huff, sizeof (huff)))
    return GST_FLOW_ERROR;

  /* *INDENT-OFF* */
  slice_param = (VASliceParameterBufferJPEGBaseline) {
    .slice_data_size = size,
    .slice_data_offset = 0,
    .slice_data_flag = VA_SLICE_DATA_FLAG_ALL,
    .slice_horizontal_position = 0,
    .slice_vertical_position = 0,
    .restart_interval = scan->restart_interval,
    .num_mcus = scan->mcu_rows_in_scan * scan->mcus_per_row,
    .num_components = scan->scan_hdr->num_components,
  };
  /* *INDENT-ON* */

  for (i = 0; i < scan->scan_hdr->num_components; i++) {
    slice_param.components[i].component_selector =
        scan->scan_hdr->components[i].component_selector;
    slice_param.components[i].dc_table_selector =
        scan->scan_hdr->components[i].dc_selector;
    slice_param.components[i].ac_table_selector =
        scan->scan_hdr->components[i].ac_selector;
  }

  if (!gst_va_decoder_add_slice_buffer (base->decoder, self->pic, &slice_param,
          sizeof (slice_param), (void *) buffer, size))
    return GST_FLOW_ERROR;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_va_jpeg_dec_end_picture (GstJpegDecoder * decoder)
{
  GstVaJpegDec *self = GST_VA_JPEG_DEC (decoder);
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);

  GST_LOG_OBJECT (self, "end picture");

  if (!gst_va_decoder_decode (base->decoder, self->pic))
    return GST_FLOW_ERROR;
  return GST_FLOW_OK;
}

static GstFlowReturn
gst_va_jpeg_dec_output_picture (GstJpegDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (decoder);

  if (gst_va_base_dec_process_output (base, frame, NULL, 0))
    return gst_video_decoder_finish_frame (vdec, frame);
  return GST_FLOW_ERROR;
}

/* @XXX: Checks for drivers that can do color convertion to nv12
 * regardless the input chroma, while it's YUV.  */
static gboolean
has_internal_nv12_color_convertion (GstVaBaseDec * base, GstVideoFormat format)
{
  if (!GST_VA_DISPLAY_IS_IMPLEMENTATION (base->display, INTEL_I965)
      && !GST_VA_DISPLAY_IS_IMPLEMENTATION (base->display, INTEL_IHD))
    return FALSE;

  if (base->rt_format != VA_RT_FORMAT_YUV420
      && base->rt_format != VA_RT_FORMAT_YUV422)
    return FALSE;

  if (format != GST_VIDEO_FORMAT_NV12)
    return FALSE;

  return TRUE;
}

static gboolean
gst_va_jpeg_dec_negotiate (GstVideoDecoder * decoder)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (decoder);
  GstVaJpegDec *self = GST_VA_JPEG_DEC (decoder);
  GstVideoFormat format;
  guint64 modifier;
  GstCapsFeatures *capsfeatures = NULL;

  /* Ignore downstream renegotiation request. */
  if (!base->need_negotiation)
    return TRUE;

  base->need_negotiation = FALSE;

  if (GST_VA_DISPLAY_IS_IMPLEMENTATION (base->display, INTEL_I965))
    base->hacks = GST_VA_HACK_SURFACE_NO_FOURCC;

  if (gst_va_decoder_is_open (base->decoder)
      && !gst_va_decoder_close (base->decoder))
    return FALSE;

  if (!gst_va_decoder_open (base->decoder, base->profile, base->rt_format))
    return FALSE;

  if (!gst_va_decoder_set_frame_size (base->decoder, base->width, base->height))
    return FALSE;

  if (base->output_state)
    gst_video_codec_state_unref (base->output_state);

  /* hack for RGBP rt_format, because only RGBP is exposed as pixel
   * format */
  if (base->rt_format == RT_FORMAT_RGB)
    base->rt_format = VA_RT_FORMAT_RGBP;

  gst_va_base_dec_get_preferred_format_and_caps_features (base, &format,
      &capsfeatures, &modifier);
  if (format == GST_VIDEO_FORMAT_UNKNOWN)
    return FALSE;

  if (!has_internal_nv12_color_convertion (base, format)
      && (gst_va_chroma_from_video_format (format) != base->rt_format))
    return FALSE;

  /* hack for RGBP rt_format */
  if (base->rt_format == VA_RT_FORMAT_RGBP)
    base->rt_format = RT_FORMAT_RGB;

  base->output_state =
      gst_video_decoder_set_output_state (decoder, format,
      base->width, base->height, base->input_state);

  /* set caps feature */
  if (capsfeatures && gst_caps_features_contains (capsfeatures,
          GST_CAPS_FEATURE_MEMORY_DMABUF)) {
    base->output_state->caps =
        gst_va_video_info_to_dma_caps (&base->output_state->info, modifier);
  } else {
    base->output_state->caps =
        gst_video_info_to_caps (&base->output_state->info);
  }

  if (capsfeatures)
    gst_caps_set_features_simple (base->output_state->caps, capsfeatures);

  GST_INFO_OBJECT (self, "Negotiated caps %" GST_PTR_FORMAT,
      base->output_state->caps);

  return GST_VIDEO_DECODER_CLASS (parent_class)->negotiate (decoder);
}

static void
gst_va_jpeg_dec_dispose (GObject * object)
{
  GstVaJpegDec *self = GST_VA_JPEG_DEC (object);

  gst_va_base_dec_close (GST_VIDEO_DECODER (object));
  g_clear_pointer (&self->pic, gst_va_decode_picture_free);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_va_jpeg_dec_class_init (gpointer g_class, gpointer class_data)
{
  GstCaps *src_doc_caps, *sink_doc_caps;
  GObjectClass *gobject_class = G_OBJECT_CLASS (g_class);
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  GstJpegDecoderClass *jpegdecoder_class = GST_JPEG_DECODER_CLASS (g_class);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (g_class);
  struct CData *cdata = class_data;
  gchar *long_name;

  if (cdata->description) {
    long_name = g_strdup_printf ("VA-API JPEG Decoder in %s",
        cdata->description);
  } else {
    long_name = g_strdup ("VA-API JPEG Decoder");
  }

  gst_element_class_set_metadata (element_class, long_name,
      "Codec/Decoder/Image/Hardware",
      "VA-API based JPEG image decoder",
      "Víctor Jáquez <vjaquez@igalia.com>");

  sink_doc_caps = gst_caps_from_string (sink_caps_str);
  src_doc_caps = gst_caps_from_string (src_caps_str);

  parent_class = g_type_class_peek_parent (g_class);

  /**
   * GstVaJpegDec:device-path:
   *
   * It shows the DRM device path used for the VA operation, if any.
   */
  gst_va_base_dec_class_init (GST_VA_BASE_DEC_CLASS (g_class), JPEG,
      cdata->render_device_path, cdata->sink_caps, cdata->src_caps,
      src_doc_caps, sink_doc_caps);

  gobject_class->dispose = gst_va_jpeg_dec_dispose;

  decoder_class->negotiate = GST_DEBUG_FUNCPTR (gst_va_jpeg_dec_negotiate);

  jpegdecoder_class->decode_scan =
      GST_DEBUG_FUNCPTR (gst_va_jpeg_dec_decode_scan);
  jpegdecoder_class->new_picture =
      GST_DEBUG_FUNCPTR (gst_va_jpeg_dec_new_picture);
  jpegdecoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_va_jpeg_dec_end_picture);
  jpegdecoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_va_jpeg_dec_output_picture);

  g_free (long_name);
  g_free (cdata->description);
  g_free (cdata->render_device_path);
  gst_caps_unref (cdata->src_caps);
  gst_caps_unref (cdata->sink_caps);
  g_free (cdata);
}

static void
gst_va_jpeg_dec_init (GTypeInstance * instance, gpointer g_class)
{
  GstVaBaseDec *base = GST_VA_BASE_DEC (instance);

  gst_va_base_dec_init (base, GST_CAT_DEFAULT);
  base->min_buffers = 1;
}

static gpointer
_register_debug_category (gpointer data)
{
  GST_DEBUG_CATEGORY_INIT (gst_va_jpegdec_debug, "vajpegdec", 0,
      "VA jpeg decoder");

  return NULL;
}

static GstCaps *
_fixup_sink_caps (GstVaDisplay * display, GstCaps * caps)
{
  if (GST_VA_DISPLAY_IS_IMPLEMENTATION (display, INTEL_I965)) {
    guint i;
    GstCaps *ret;
    GValue sampling = G_VALUE_INIT;
    const char *sampling_list[] = { "YCbCr-4:2:0", "YCbCr-4:2:2" };

    ret = gst_caps_copy (caps);
    gst_caps_set_simple (ret, "colorspace", G_TYPE_STRING, "sYUV", NULL);

    gst_value_list_init (&sampling, G_N_ELEMENTS (sampling_list));
    for (i = 0; i < G_N_ELEMENTS (sampling_list); i++) {
      GValue samp = G_VALUE_INIT;
      g_value_init (&samp, G_TYPE_STRING);
      g_value_set_string (&samp, sampling_list[i]);
      gst_value_list_append_value (&sampling, &samp);
      g_value_unset (&samp);
    }

    gst_caps_set_value (ret, "sampling", &sampling);
    g_value_unset (&sampling);
    return ret;
  }

  /* TODO: support interleaved mjpeg */
  gst_caps_set_simple (caps, "interlace-mode", G_TYPE_STRING, "progressive",
      NULL);
  return gst_caps_ref (caps);
}

static GstCaps *
_fixup_src_caps (GstVaDisplay * display, GstCaps * caps)
{
  if (GST_VA_DISPLAY_IS_IMPLEMENTATION (display, INTEL_IHD)) {
    GstCaps *ret;
    guint i, len;

    ret = gst_caps_copy (caps);

    len = gst_caps_get_size (ret);
    for (i = 0; i < len; i++) {
      guint j, size;
      GValue out = G_VALUE_INIT;
      const GValue *in;
      GstStructure *s;
      GstCapsFeatures *f;

      f = gst_caps_get_features (ret, i);
      if (!gst_caps_features_is_equal (f,
              GST_CAPS_FEATURES_MEMORY_SYSTEM_MEMORY))
        continue;

      s = gst_caps_get_structure (ret, i);

      in = gst_structure_get_value (s, "format");

      size = gst_value_list_get_size (in);
      gst_value_list_init (&out, size);
      for (j = 0; j < size; j++) {
        const GValue *fmt = gst_value_list_get_value (in, j);

        /* rgbp is not correctly mapped into memory */
        if (g_strcmp0 (g_value_get_string (fmt), "RGBP") != 0)
          gst_value_list_append_value (&out, fmt);
      }
      gst_structure_take_value (s, "format", &out);
    }

    return ret;
  } else if (GST_VA_DISPLAY_IS_IMPLEMENTATION (display, INTEL_I965)) {
    GstCaps *ret;
    GstStructure *s;
    GstCapsFeatures *f;
    guint i, len;

    ret = gst_caps_copy (caps);

    len = gst_caps_get_size (ret);
    for (i = 0; i < len; i++) {
      s = gst_caps_get_structure (ret, i);
      f = gst_caps_get_features (ret, i);

      /* DMA kind formats have modifiers, we should not change */
      if (gst_caps_features_contains (f, GST_CAPS_FEATURE_MEMORY_DMABUF))
        continue;

      /* only NV12 works in this nigthmare */
      gst_structure_set (s, "format", G_TYPE_STRING, "NV12", NULL);
    }

    return ret;
  }
  return gst_caps_ref (caps);
}

gboolean
gst_va_jpeg_dec_register (GstPlugin * plugin, GstVaDevice * device,
    GstCaps * sink_caps, GstCaps * src_caps, guint rank)
{
  static GOnce debug_once = G_ONCE_INIT;
  GType type;
  GTypeInfo type_info = {
    .class_size = sizeof (GstVaJpegDecClass),
    .class_init = gst_va_jpeg_dec_class_init,
    .instance_size = sizeof (GstVaJpegDec),
    .instance_init = gst_va_jpeg_dec_init,
  };
  struct CData *cdata;
  gboolean ret;
  gchar *type_name, *feature_name;

  g_return_val_if_fail (GST_IS_PLUGIN (plugin), FALSE);
  g_return_val_if_fail (GST_IS_VA_DEVICE (device), FALSE);
  g_return_val_if_fail (GST_IS_CAPS (sink_caps), FALSE);
  g_return_val_if_fail (GST_IS_CAPS (src_caps), FALSE);

  cdata = g_new (struct CData, 1);
  cdata->description = NULL;
  cdata->render_device_path = g_strdup (device->render_device_path);
  cdata->sink_caps = _fixup_sink_caps (device->display, sink_caps);
  cdata->src_caps = _fixup_src_caps (device->display, src_caps);

  /* class data will be leaked if the element never gets instantiated */
  GST_MINI_OBJECT_FLAG_SET (cdata->sink_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (cdata->src_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  type_info.class_data = cdata;

  gst_va_create_feature_name (device, "GstVaJpegDec", "GstVa%sJpegDec",
      &type_name, "vajpegdec", "va%sjpegdec", &feature_name,
      &cdata->description, &rank);

  g_once (&debug_once, _register_debug_category, NULL);

  type = g_type_register_static (GST_TYPE_JPEG_DECODER,
      type_name, &type_info, 0);

  ret = gst_element_register (plugin, feature_name, rank, type);

  g_free (type_name);
  g_free (feature_name);

  return ret;
}
