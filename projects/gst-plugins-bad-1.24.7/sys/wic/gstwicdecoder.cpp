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

#include "gstwicdecoder.h"

#include <wrl.h>
#include <string.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */


GST_DEBUG_CATEGORY_STATIC (gst_wic_decoder_debug);
#define GST_CAT_DEFAULT gst_wic_decoder_debug

struct _GstWicDecoderPrivate
{
  GstWicImagingFactory *factory;
  IStream *stream;
};

static gboolean gst_wic_decoder_open (GstVideoDecoder * decoder);
static gboolean gst_wic_decoder_close (GstVideoDecoder * decoder);
static gboolean gst_wic_decoder_stop (GstVideoDecoder * decoder);
static gboolean gst_wic_decoder_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static GstFlowReturn gst_wic_decoder_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);

#define gst_wic_decoder_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GstWicDecoder, gst_wic_decoder,
    GST_TYPE_VIDEO_DECODER);

static void
gst_wic_decoder_class_init (GstWicDecoderClass * klass)
{
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);

  decoder_class->open = GST_DEBUG_FUNCPTR (gst_wic_decoder_open);
  decoder_class->close = GST_DEBUG_FUNCPTR (gst_wic_decoder_close);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_wic_decoder_stop);
  decoder_class->set_format = GST_DEBUG_FUNCPTR (gst_wic_decoder_set_format);
  decoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_wic_decoder_handle_frame);

  GST_DEBUG_CATEGORY_INIT (gst_wic_decoder_debug,
      "wicdecoder", 0, "wicdecoder");

  gst_type_mark_as_plugin_api (GST_TYPE_WIC_DECODER, (GstPluginAPIFlags) 0);
}

static void
gst_wic_decoder_init (GstWicDecoder * self)
{
  self->priv =
      (GstWicDecoderPrivate *) gst_wic_decoder_get_instance_private (self);
}

static gboolean
gst_wic_decoder_open (GstVideoDecoder * decoder)
{
  GstWicDecoder *self = GST_WIC_DECODER (decoder);
  GstWicDecoderClass *klass = GST_WIC_DECODER_GET_CLASS (self);
  GstWicDecoderPrivate *priv = self->priv;
  HRESULT hr;
  ComPtr < IStream > stream;

  priv->factory = gst_wic_imaging_factory_new ();
  if (!priv->factory) {
    GST_ERROR_OBJECT (self, "Failed to create factory");
    return FALSE;
  }

  hr = gst_wic_imaging_factory_check_codec_support (priv->factory,
      TRUE, klass->codec_id);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Codec is not supported, hr: 0x%x", (guint) hr);
    goto error;
  }

  hr = CreateStreamOnHGlobal (nullptr, TRUE, &stream);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Failed to create IStream object");
    goto error;
  }

  priv->stream = stream.Detach ();

  return TRUE;

error:
  GST_WIC_CLEAR_COM (priv->stream);
  gst_clear_object (&priv->factory);

  return FALSE;
}

static gboolean
gst_wic_decoder_close (GstVideoDecoder * decoder)
{
  GstWicDecoder *self = GST_WIC_DECODER (decoder);
  GstWicDecoderPrivate *priv = self->priv;

  GST_WIC_CLEAR_COM (priv->stream);
  gst_clear_object (&priv->factory);

  return TRUE;
}

static gboolean
gst_wic_decoder_stop (GstVideoDecoder * decoder)
{
  GstWicDecoder *self = GST_WIC_DECODER (decoder);

  g_clear_pointer (&self->input_state, gst_video_codec_state_unref);

  return TRUE;
}

static gboolean
gst_wic_decoder_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state)
{
  GstWicDecoder *self = GST_WIC_DECODER (decoder);
  GstWicDecoderClass *klass = GST_WIC_DECODER_GET_CLASS (self);

  self->input_state = gst_video_codec_state_ref (state);

  if (klass->set_format)
    return klass->set_format (self, state);

  return TRUE;
}

static gboolean
gst_wic_decoder_upload (GstWicDecoder * self, GstBuffer * buffer)
{
  GstWicDecoderPrivate *priv = self->priv;
  IStream *stream = priv->stream;
  LARGE_INTEGER pos;
  ULARGE_INTEGER size;
  HRESULT hr;
  GstMapInfo info;

  if (!gst_buffer_map (buffer, &info, GST_MAP_READ)) {
    GST_ELEMENT_ERROR (self,
        RESOURCE, READ, ("Unable to map buffer"), (nullptr));
    return FALSE;
  }

  size.QuadPart = info.size;
  hr = stream->SetSize (size);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self,
        "Failed to set size to %" G_GSIZE_FORMAT, info.size);
    goto read_write_error;
  }

  pos.QuadPart = 0;
  hr = stream->Seek (pos, STREAM_SEEK_SET, nullptr);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Failed to seek IStream");
    gst_buffer_unmap (buffer, &info);
    goto read_write_error;
  }

  hr = stream->Write (info.data, info.size, nullptr);

  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Failed to write data into IStream");
    goto read_write_error;
  }

  hr = stream->Seek (pos, STREAM_SEEK_SET, nullptr);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Failed to seek IStream");
    goto read_write_error;
  }

  gst_buffer_unmap (buffer, &info);

  return TRUE;

read_write_error:
  {
    gchar *error_text = g_win32_error_message ((guint) hr);

    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ_WRITE,
        ("Failed to read/write stream for decoding"),
        ("HRSULT: 0x%x (%s)", (guint) hr, GST_STR_NULL (error_text)));
    g_free (error_text);

    gst_buffer_unmap (buffer, &info);

    return FALSE;
  }
}

static gboolean
gst_wic_decoder_decode (GstWicDecoder * self, IWICBitmapDecoder ** decoder)
{
  GstWicDecoderClass *klass = GST_WIC_DECODER_GET_CLASS (self);
  GstWicDecoderPrivate *priv = self->priv;
  IStream *stream = priv->stream;
  IWICImagingFactory *factory;
  HRESULT hr;
  ComPtr < IWICBitmapDecoder > handle;

  factory = gst_wic_imaging_factory_get_handle (priv->factory);
  hr = factory->CreateDecoder (klass->codec_id, nullptr, &handle);
  if (FAILED (hr)) {
    GST_ELEMENT_ERROR (self, STREAM, DECODE, ("Unable to create decoder"),
        ("IWICImagingFactory::IWICBitmapDecoder returned hr 0x%x", (guint) hr));
    return FALSE;
  }

  hr = handle->Initialize (stream, WICDecodeMetadataCacheOnLoad);
  if (FAILED (hr)) {
    GST_ELEMENT_ERROR (self, STREAM, DECODE, ("Unable initialize decoder"),
        ("IWICBitmapDecoder::Initialize returned hr 0x%x", (guint) hr));
    return FALSE;
  }

  *decoder = handle.Detach ();

  return TRUE;
}

static GstFlowReturn
gst_wic_decoder_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstWicDecoder *self = GST_WIC_DECODER (decoder);
  GstWicDecoderClass *klass = GST_WIC_DECODER_GET_CLASS (self);
  GstWicDecoderPrivate *priv = self->priv;
  ComPtr < IWICBitmapDecoder > handle;
  ComPtr < IWICBitmapFrameDecode > decode_frame;
  HRESULT hr;
  IWICImagingFactory *factory;

  factory = gst_wic_imaging_factory_get_handle (priv->factory);

  if (!gst_wic_decoder_upload (self, frame->input_buffer)) {
    gst_video_decoder_release_frame (decoder, frame);
    return GST_FLOW_ERROR;
  }

  if (!gst_wic_decoder_decode (self, &handle)) {
    gst_video_decoder_release_frame (decoder, frame);
    return GST_FLOW_ERROR;
  }

  hr = handle->GetFrame (0, &decode_frame);
  if (FAILED (hr)) {
    gst_video_decoder_release_frame (decoder, frame);

    GST_ELEMENT_ERROR (self, STREAM, DECODE, ("Failed to decode"),
        ("IWICBitmapDecoder::GetFrame returned hr 0x%x", (guint) hr));

    return GST_FLOW_ERROR;
  }

  return klass->process_output (self, factory, decode_frame.Get (), frame);
}
