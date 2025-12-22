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
 * SECTION:element-wicjpegdec
 * @title: wicjpegdec
 *
 * This element decodes JPEG compressed data into RAW video data.
 *
 * Since: 1.22
 *
 */

#include "gstwicjpegdec.h"

#include <wrl.h>
#include <string.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_STATIC (gst_wic_jpeg_dec_debug);
#define GST_CAT_DEFAULT gst_wic_jpeg_dec_debug

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/jpeg")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ BGR, GRAY8, NV12, Y42B, Y444 }"))
    );


struct _GstWicJpegDec
{
  GstWicDecoder parent;

  WICBitmapPlaneDescription plane_desc[GST_VIDEO_MAX_PLANES];

  GstVideoInfo info;
};

static gboolean gst_wic_jpeg_dec_set_format (GstWicDecoder * decoder,
    GstVideoCodecState * state);
static GstFlowReturn gst_wic_jpeg_dec_process_output (GstWicDecoder * decoder,
    IWICImagingFactory * factory, IWICBitmapFrameDecode * decode_frame,
    GstVideoCodecFrame * frame);

#define gst_wic_decoder_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstWicJpegDec, gst_wic_jpeg_dec, GST_TYPE_WIC_DECODER,
    GST_DEBUG_CATEGORY_INIT (gst_wic_jpeg_dec_debug,
        "wicjpegdec", 0, "wicjpegdec"));

static void
gst_wic_jpeg_dec_class_init (GstWicJpegDecClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstWicDecoderClass *decoder_class = GST_WIC_DECODER_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);

  gst_element_class_set_static_metadata (element_class,
      "Windows Imaging Component JPEG decoder", "Codec/Decoder/Image",
      "Jpeg image decoder using Windows Imaging Component API",
      "Seungha Yang <seungha@centricular.com>");

  decoder_class->codec_id = GUID_ContainerFormatJpeg;
  decoder_class->set_format = GST_DEBUG_FUNCPTR (gst_wic_jpeg_dec_set_format);
  decoder_class->process_output =
      GST_DEBUG_FUNCPTR (gst_wic_jpeg_dec_process_output);
}

static void
gst_wic_jpeg_dec_init (GstWicJpegDec * self)
{
  gst_video_info_init (&self->info);
  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (self), TRUE);
}

static gboolean
gst_wic_jpeg_dec_prepare_yuv_output (GstWicJpegDec * self,
    WICJpegFrameHeader * hdr,
    IWICBitmapSource * input, guint out_width, guint out_height,
    IWICPlanarBitmapSourceTransform ** transform)
{
  ComPtr < IWICPlanarBitmapSourceTransform > tr;

  HRESULT hr;
  BOOL is_supported = FALSE;
  UINT32 supported_width = out_width;
  UINT32 supported_height = out_height;
  const WICPixelFormatGUID yuv_planar_formats[] = {
    GUID_WICPixelFormat8bppY,
    GUID_WICPixelFormat8bppCb,
    GUID_WICPixelFormat8bppCr
  };
  const WICPixelFormatGUID nv12_formats[] = {
    GUID_WICPixelFormat8bppY,
    GUID_WICPixelFormat16bppCbCr,
  };
  const WICPixelFormatGUID *dst_formats = yuv_planar_formats;
  guint n_planes = G_N_ELEMENTS (yuv_planar_formats);
  GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;

  switch (hdr->SampleFactors) {
    case WIC_JPEG_SAMPLE_FACTORS_ONE:
      /* GRAY */
      return FALSE;
    case WIC_JPEG_SAMPLE_FACTORS_THREE_420:
      /* NV12 is preferred over I420 on Windows, because I420 is not suppported
       * by various Windows APIs, Specifically DXGI doesn't support I420
       * natively */
      format = GST_VIDEO_FORMAT_NV12;
      dst_formats = nv12_formats;
      n_planes = G_N_ELEMENTS (nv12_formats);
      break;
    case WIC_JPEG_SAMPLE_FACTORS_THREE_422:
      format = GST_VIDEO_FORMAT_Y42B;
      break;
    case WIC_JPEG_SAMPLE_FACTORS_THREE_444:
      format = GST_VIDEO_FORMAT_Y444;
      break;
    case WIC_JPEG_SAMPLE_FACTORS_THREE_440:
      /* We don't support this format */
      return FALSE;
    default:
      return FALSE;
  }

  hr = input->QueryInterface (IID_PPV_ARGS (&tr));
  if (FAILED (hr)) {
    GST_TRACE_OBJECT (self, "IWICPlanarBitmapSourceTransform is not supported");
    return FALSE;
  }

  hr = tr->DoesSupportTransform (&supported_width,
      &supported_height,
      WICBitmapTransformRotate0,
      WICPlanarOptionsPreserveSubsampling,
      dst_formats, self->plane_desc, n_planes, &is_supported);

  if (FAILED (hr) || !is_supported) {
    GST_TRACE_OBJECT (self, "Transform is not supported");
    return FALSE;
  }

  GST_LOG_OBJECT (self, "Transform supported %dx%d -> %dx%d",
      out_width, out_height, supported_width, supported_height);
  for (guint i = 0; i < n_planes; i++) {
    GST_LOG_OBJECT (self, "Plane %d, %dx%d", i,
        self->plane_desc[i].Width, self->plane_desc[i].Height);
  }

  gst_video_info_set_format (&self->info, format, supported_width,
      supported_height);

  *transform = tr.Detach ();

  return TRUE;
}

static gboolean
gst_wic_jpeg_dec_prepare_rgb_output (GstWicJpegDec * self,
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

  /* native output formats are BGR, GRAY and CMYK but we don't support
   * CMYK */
  if (!gst_wic_pixel_format_to_gst (native_pixel_format, &native_format) ||
      (native_format != GST_VIDEO_FORMAT_BGR &&
          native_format != GST_VIDEO_FORMAT_GRAY8)) {
    ComPtr < IWICFormatConverter > conv;

    GST_LOG_OBJECT (self,
        "Native format is not supported for output, needs conversion");

    native_format = GST_VIDEO_FORMAT_BGR;

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
gst_wic_jpeg_dec_fill_yuv_output (GstWicJpegDec * self,
    IWICImagingFactory * factory, IWICPlanarBitmapSourceTransform * transform,
    GstBuffer * buffer)
{
  ComPtr < IWICBitmap > bitmaps[GST_VIDEO_MAX_PLANES];
  ComPtr < IWICBitmapLock > locks[GST_VIDEO_MAX_PLANES];
  WICBitmapPlane planes[GST_VIDEO_MAX_PLANES];
  HRESULT hr;
  GstVideoFrame frame;
  guint num_planes = GST_VIDEO_INFO_N_PLANES (&self->info);

  for (guint i = 0; i < num_planes; i++) {
    hr = factory->CreateBitmap (self->plane_desc[i].Width,
        self->plane_desc[i].Height,
        self->plane_desc[i].Format, WICBitmapCacheOnLoad, &bitmaps[i]);
    if (FAILED (hr)) {
      GST_ERROR_OBJECT (self, "Failed to create bitmap, hr: 0x%x", (guint) hr);
      return FALSE;
    }

    hr = gst_wic_lock_bitmap (bitmaps[i].Get (), nullptr,
        WICBitmapLockRead | WICBitmapLockWrite, &locks[i], &planes[i]);
    if (FAILED (hr)) {
      GST_ERROR_OBJECT (self, "Failed to lock bitmap for plane %d", i);
      return FALSE;
    }
  }

  hr = transform->CopyPixels (nullptr, self->info.width, self->info.height,
      WICBitmapTransformRotate0, WICPlanarOptionsPreserveSubsampling, planes,
      num_planes);

  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Failed to copy pixels, hr: 0x%x", (guint) hr);
    return FALSE;
  }

  if (!gst_video_frame_map (&frame, &self->info, buffer, GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (self, "Failed to map output buffer");
    return FALSE;
  }

  for (guint i = 0; i < num_planes; i++) {
    WICBitmapPlane *plane = &planes[i];
    guint height, width_in_bytes;
    guint8 *src, *dst;
    guint src_stride, dst_stride;

    src = plane->pbBuffer;
    dst = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, i);

    src_stride = plane->cbStride;
    dst_stride = GST_VIDEO_FRAME_PLANE_STRIDE (&frame, i);

    width_in_bytes = GST_VIDEO_FRAME_COMP_WIDTH (&frame, i) *
        GST_VIDEO_FRAME_COMP_PSTRIDE (&frame, i);
    height = GST_VIDEO_FRAME_COMP_HEIGHT (&frame, i);

    for (guint j = 0; j < height; j++) {
      memcpy (dst, src, width_in_bytes);
      src += src_stride;
      dst += dst_stride;
    }
  }

  gst_video_frame_unmap (&frame);

  return TRUE;
}

static gboolean
gst_wic_jpeg_dec_fill_rgb_output (GstWicJpegDec * self,
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
gst_wic_jpeg_dec_update_output_state (GstWicJpegDec * self)
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

  /* Update colorimetry and chroma-site if upstream is not specified */
  if (GST_VIDEO_INFO_IS_YUV (info)) {
    GstStructure *s = gst_caps_get_structure (wic->input_state->caps, 0);

    if (wic->input_state->info.chroma_site == GST_VIDEO_CHROMA_SITE_UNKNOWN)
      output_state->info.chroma_site = GST_VIDEO_CHROMA_SITE_NONE;

    if (!gst_structure_get_string (s, "colorimetry")) {
      output_state->info.colorimetry.range = GST_VIDEO_COLOR_RANGE_0_255;
      output_state->info.colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_BT601;
      output_state->info.colorimetry.transfer = GST_VIDEO_TRANSFER_UNKNOWN;
      output_state->info.colorimetry.primaries =
          GST_VIDEO_COLOR_PRIMARIES_UNKNOWN;
    }
  }

  gst_video_codec_state_unref (output_state);
  gst_video_decoder_negotiate (vdec);

  return;
}

static gboolean
gst_wic_jpeg_dec_set_format (GstWicDecoder * decoder,
    GstVideoCodecState * state)
{
  GstWicJpegDec *self = GST_WIC_JPEG_DEC (decoder);

  gst_video_info_init (&self->info);

  return TRUE;
}

static GstFlowReturn
gst_wic_jpeg_dec_process_output (GstWicDecoder * decoder,
    IWICImagingFactory * factory, IWICBitmapFrameDecode * decode_frame,
    GstVideoCodecFrame * frame)
{
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (decoder);
  GstWicJpegDec *self = GST_WIC_JPEG_DEC (decoder);
  GstVideoInfo *info = &decoder->input_state->info;
  ComPtr < IWICBitmapSource > source;
  ComPtr < IWICBitmapSource > input;
  ComPtr < IWICPlanarBitmapSourceTransform > transform;
  ComPtr < IWICJpegFrameDecode > jpeg_decode;
  UINT width, height;
  HRESULT hr;
  guint out_width, out_height;
  GstFlowReturn flow_ret;
  gboolean rst;
  WICJpegFrameHeader hdr;

  hr = decode_frame->GetSize (&width, &height);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (decoder, "Failed to get size, hr: 0x%x", (guint) hr);
    goto error;
  }

  hr = decode_frame->QueryInterface (IID_PPV_ARGS (&jpeg_decode));
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "IWICJpegFrameDecode interface is not supported");
    goto error;
  }

  hr = jpeg_decode->GetFrameHeader (&hdr);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Failed to get frame header, hr:0x%x", (guint) hr);
    goto error;
  }

  /* JPEG may have interlaced stream, but WIC supports only single jpeg
   * frame per run (other field may be dropped), configure scaler to
   * workaround it */
  if (width == info->width && 2 * height == info->height) {
    ComPtr < IWICBitmapScaler > scaler;

    GST_LOG_OBJECT (decoder,
        "Need scale %dx%d -> %dx%d", width, height, info->width, info->height);
    out_width = info->width;
    out_height = info->height;

    hr = factory->CreateBitmapScaler (&scaler);
    if (FAILED (hr)) {
      GST_ERROR_OBJECT (self, "Failed to create scaler, hr: 0x%x", (guint) hr);
      goto error;
    }

    hr = scaler->Initialize (decode_frame, out_width, out_height,
        WICBitmapInterpolationModeHighQualityCubic);
    if (FAILED (hr)) {
      GST_ERROR_OBJECT (self, "Unable to initialize scaler, hr: 0x%x",
          (guint) hr);
      goto error;
    }

    scaler.As (&input);
  } else {
    out_width = width;
    out_height = height;
    input = decode_frame;
  }

  /* WIC JPEG decoder supports GRAY8, BGR and CMYK pixel formats as native
   * output formats, and staring with Windows 8.1, YUV formats support was added.
   * See also
   * https://docs.microsoft.com/en-us/windows/win32/wic/-wic-codec-native-pixel-formats#jpeg-native-codec
   * https://docs.microsoft.com/en-us/windows/win32/wic/jpeg-ycbcr-support
   *
   * This element will output the native pixel format if possible, but
   * conversion will/should happen for 4:4:0 YUV or CMYK since we don't have any
   * defined format for those formats
   */
  rst = gst_wic_jpeg_dec_prepare_yuv_output (self,
      &hdr, input.Get (), out_width, out_height, &transform);

  if (!rst) {
    rst = gst_wic_jpeg_dec_prepare_rgb_output (self, factory, input.Get (),
        out_width, out_height, &source);
  }

  if (!rst)
    goto error;

  gst_wic_jpeg_dec_update_output_state (self);

  flow_ret = gst_video_decoder_allocate_output_frame (vdec, frame);
  if (flow_ret != GST_FLOW_OK) {
    gst_video_decoder_release_frame (vdec, frame);
    GST_INFO_OBJECT (self, "Unable to allocate output");
    return flow_ret;
  }

  if (transform) {
    rst = gst_wic_jpeg_dec_fill_yuv_output (self, factory, transform.Get (),
        frame->output_buffer);
  } else {
    rst = gst_wic_jpeg_dec_fill_rgb_output (self, factory, source.Get (),
        frame->output_buffer);
  }

  if (!rst)
    goto error;

  return gst_video_decoder_finish_frame (vdec, frame);

error:
  gst_video_decoder_release_frame (vdec, frame);
  return GST_FLOW_ERROR;
}
