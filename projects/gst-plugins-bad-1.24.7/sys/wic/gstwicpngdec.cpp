/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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
 * SECTION:element-wicpngdec
 * @title: wicpngdec
 *
 * This element decodes PNG compressed data into RAW video data.
 *
 * Since: 1.22
 *
 */

#include "gstwicpngdec.h"

#include <wrl.h>
#include <string.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_STATIC (gst_wic_png_dec_debug);
#define GST_CAT_DEFAULT gst_wic_png_dec_debug

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/png")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ RGBA64_LE, BGRA, RGBA, BGR, RGB, GRAY8, GRAY16_BE }"))
    );


struct _GstWicPngDec
{
  GstWicDecoder parent;

  WICBitmapPlaneDescription plane_desc[GST_VIDEO_MAX_PLANES];

  GstVideoInfo info;
};

static gboolean gst_wic_png_dec_set_format (GstWicDecoder * decoder,
    GstVideoCodecState * state);
static GstFlowReturn gst_wic_png_dec_process_output (GstWicDecoder * decoder,
    IWICImagingFactory * factory, IWICBitmapFrameDecode * decode_frame,
    GstVideoCodecFrame * frame);

#define gst_wic_decoder_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstWicPngDec, gst_wic_png_dec, GST_TYPE_WIC_DECODER,
    GST_DEBUG_CATEGORY_INIT (gst_wic_png_dec_debug,
        "wicpngdec", 0, "wicpngdec"));

static void
gst_wic_png_dec_class_init (GstWicPngDecClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstWicDecoderClass *decoder_class = GST_WIC_DECODER_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);

  gst_element_class_set_static_metadata (element_class,
      "Windows Imaging Component PNG decoder", "Codec/Decoder/Image",
      "Png image decoder using Windows Imaging Component API",
      "Seungha Yang <seungha@centricular.com>");

  decoder_class->codec_id = GUID_ContainerFormatPng;
  decoder_class->set_format = GST_DEBUG_FUNCPTR (gst_wic_png_dec_set_format);
  decoder_class->process_output =
      GST_DEBUG_FUNCPTR (gst_wic_png_dec_process_output);
}

static void
gst_wic_png_dec_init (GstWicPngDec * self)
{
  gst_video_info_init (&self->info);
  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (self), TRUE);
}

static gboolean
gst_wic_png_dec_prepare_output (GstWicPngDec * self,
    IWICImagingFactory * factory, IWICBitmapSource * input,
    guint out_width, guint out_height, IWICBitmapSource ** source)
{
  WICPixelFormatGUID native_pixel_format;
  GstVideoFormat native_format = GST_VIDEO_FORMAT_UNKNOWN;
  HRESULT hr;
  ComPtr < IWICBitmapSource > output;

  hr = input->GetPixelFormat (&native_pixel_format);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Failed to query pixel format, hr: 0x%x",
        (guint) hr);
    return FALSE;
  }

  if (!gst_wic_pixel_format_to_gst (native_pixel_format, &native_format)) {
    ComPtr < IWICFormatConverter > conv;

    GST_LOG_OBJECT (self,
        "Native format is not supported for output, needs conversion");

    native_format = GST_VIDEO_FORMAT_BGRA;
    if (IsEqualGUID (native_pixel_format, GUID_WICPixelFormat1bppIndexed)
        || IsEqualGUID (native_pixel_format, GUID_WICPixelFormat2bppIndexed)
        || IsEqualGUID (native_pixel_format, GUID_WICPixelFormat4bppIndexed)
        || IsEqualGUID (native_pixel_format, GUID_WICPixelFormat8bppIndexed)) {
      /* palette, convert to BGRA */
      native_format = GST_VIDEO_FORMAT_BGRA;
    } else if (IsEqualGUID (native_pixel_format, GUID_WICPixelFormatBlackWhite)
        || IsEqualGUID (native_pixel_format, GUID_WICPixelFormat2bppGray)
        || IsEqualGUID (native_pixel_format, GUID_WICPixelFormat4bppGray)) {
      /* gray scale */
      native_format = GST_VIDEO_FORMAT_GRAY8;
    } else if (IsEqualGUID (native_pixel_format, GUID_WICPixelFormat48bppRGB)) {
      /* 16bits per channel RGB, do we have defined format? */
      native_format = GST_VIDEO_FORMAT_RGBA64_LE;
    }

    if (!gst_wic_pixel_format_from_gst (native_format, &native_pixel_format)) {
      GST_ERROR_OBJECT (self, "Failed to convert format to WIC");
      return FALSE;
    }

    hr = factory->CreateFormatConverter (&conv);
    if (FAILED (hr)) {
      GST_ERROR_OBJECT (self, "Failed to create converter");
      return FALSE;
    }

    hr = conv->Initialize (input, native_pixel_format,
        WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);

    if (FAILED (hr)) {
      GST_ERROR_OBJECT (self, "Failed to initialize converter, hr: 0x%x",
          (guint) hr);
      return FALSE;
    }

    conv.As (&output);
  } else {
    output = input;
  }

  gst_video_info_set_format (&self->info, native_format, out_width, out_height);

  *source = output.Detach ();

  return TRUE;
}

static gboolean
gst_wic_png_dec_fill_output (GstWicPngDec * self,
    IWICImagingFactory * factory, IWICBitmapSource * source, GstBuffer * buffer)
{
  ComPtr < IWICBitmap > bitmap;
  ComPtr < IWICBitmapLock > bitmap_lock;
  WICBitmapPlane plane;
  HRESULT hr;
  GstVideoFrame frame;
  guint8 *src, *dst;
  guint src_stride, dst_stride;
  guint height, width_in_bytes;

  hr = factory->CreateBitmapFromSource (source, WICBitmapCacheOnDemand,
      &bitmap);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Failed to create bitmap from source, hr: 0x%x",
        (guint) hr);
    return FALSE;
  }

  hr = gst_wic_lock_bitmap (bitmap.Get (), nullptr,
      WICBitmapLockRead, &bitmap_lock, &plane);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Failed to lock bitmap");
    return FALSE;
  }

  if (!gst_video_frame_map (&frame, &self->info, buffer, GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (self, "Failed to map output buffer");
    return FALSE;
  }

  src = plane.pbBuffer;
  dst = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);

  src_stride = plane.cbStride;
  dst_stride = GST_VIDEO_FRAME_PLANE_STRIDE (&frame, 0);

  width_in_bytes =
      GST_VIDEO_FRAME_COMP_WIDTH (&frame, 0) *
      GST_VIDEO_FRAME_COMP_PSTRIDE (&frame, 0);
  height = GST_VIDEO_FRAME_COMP_HEIGHT (&frame, 0);

  for (guint i = 0; i < height; i++) {
    memcpy (dst, src, width_in_bytes);
    src += src_stride;
    dst += dst_stride;
  }

  gst_video_frame_unmap (&frame);

  return TRUE;
}

static void
gst_wic_png_dec_update_output_state (GstWicPngDec * self)
{
  GstWicDecoder *wic = GST_WIC_DECODER (self);
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (self);
  GstVideoCodecState *output_state;
  GstVideoInfo *info = &self->info;
  GstVideoInfo *output_info;

  output_state = gst_video_decoder_get_output_state (vdec);
  if (output_state) {
    output_info = &output_state->info;
    if (GST_VIDEO_INFO_FORMAT (output_info) == GST_VIDEO_INFO_FORMAT (info) &&
        GST_VIDEO_INFO_WIDTH (output_info) == GST_VIDEO_INFO_WIDTH (info) &&
        GST_VIDEO_INFO_HEIGHT (output_info) == GST_VIDEO_INFO_HEIGHT (info)) {
      gst_video_codec_state_unref (output_state);
      return;
    }
    gst_video_codec_state_unref (output_state);
  }

  output_state = gst_video_decoder_set_output_state (vdec,
      GST_VIDEO_INFO_FORMAT (info), GST_VIDEO_INFO_WIDTH (info),
      GST_VIDEO_INFO_HEIGHT (info), wic->input_state);

  gst_video_codec_state_unref (output_state);
  gst_video_decoder_negotiate (vdec);

  return;
}

static gboolean
gst_wic_png_dec_set_format (GstWicDecoder * decoder, GstVideoCodecState * state)
{
  GstWicPngDec *self = GST_WIC_PNG_DEC (decoder);

  gst_video_info_init (&self->info);

  return TRUE;
}

static GstFlowReturn
gst_wic_png_dec_process_output (GstWicDecoder * decoder,
    IWICImagingFactory * factory, IWICBitmapFrameDecode * decode_frame,
    GstVideoCodecFrame * frame)
{
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (decoder);
  GstWicPngDec *self = GST_WIC_PNG_DEC (decoder);
  ComPtr < IWICBitmapSource > source;
  ComPtr < IWICPlanarBitmapSourceTransform > transform;
  UINT width, height;
  HRESULT hr;
  GstFlowReturn flow_ret;
  gboolean rst;

  hr = decode_frame->GetSize (&width, &height);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (decoder, "Failed to get size, hr: 0x%x", (guint) hr);
    goto error;
  }

  rst = gst_wic_png_dec_prepare_output (self, factory, decode_frame,
      width, height, &source);
  if (!rst)
    goto error;

  gst_wic_png_dec_update_output_state (self);

  flow_ret = gst_video_decoder_allocate_output_frame (vdec, frame);
  if (flow_ret != GST_FLOW_OK) {
    gst_video_decoder_release_frame (vdec, frame);
    GST_INFO_OBJECT (self, "Unable to allocate output");
    return flow_ret;
  }

  rst = gst_wic_png_dec_fill_output (self, factory, source.Get (),
      frame->output_buffer);
  if (!rst)
    goto error;

  return gst_video_decoder_finish_frame (vdec, frame);

error:
  gst_video_decoder_release_frame (vdec, frame);
  return GST_FLOW_ERROR;
}
