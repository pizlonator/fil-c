/*
 * Copyright (C) 2018 Collabora Ltd.
 *   Author: Xavier Claessens <xavier.claessens@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstamc-internal-ml.h"
#include "gstamc-surfacetexture-ml.h"
#include "../gstamc-codec.h"
#include "../gstamc-constants.h"

#include <ml_media_codec.h>

struct _GstAmcCodec
{
  MLHandle handle;
  GstAmcSurfaceTexture *surface_texture;
};

static void
gst_amc_buffer_ml_free (GstAmcBuffer * buffer)
{
  g_free (buffer);
}

static gboolean
gst_amc_buffer_ml_set_position_and_limit (GstAmcBuffer * buffer, GError ** err,
    gint position, gint limit)
{
/* FIXME: Do we need to do something?
  buffer->data = buffer->data + position;
  buffer->size = limit;
*/
  return TRUE;
}

static GstAmcCodec *
gst_amc_codec_ml_new (const gchar * name, gboolean is_encoder, GError ** err)
{
  GstAmcCodec *codec = NULL;
  MLResult result;
  MLMediaCodecType type;

  g_return_val_if_fail (name != NULL, NULL);

  codec = g_slice_new0 (GstAmcCodec);
  codec->handle = ML_INVALID_HANDLE;
  type = is_encoder ? MLMediaCodecType_Encoder : MLMediaCodecType_Decoder;
  result =
      MLMediaCodecCreateCodec (MLMediaCodecCreation_ByName, type, name,
      &codec->handle);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to create codec by name %s: %d", name, result);
    gst_amc_codec_free (codec);
    return NULL;
  }

  return codec;
}

static void
gst_amc_codec_ml_free (GstAmcCodec * codec)
{
  g_return_if_fail (codec != NULL);

  if (codec->handle != ML_INVALID_HANDLE)
    MLMediaCodecDestroy (codec->handle);
  g_clear_object (&codec->surface_texture);
  g_slice_free (GstAmcCodec, codec);
}

static gboolean
gst_amc_codec_ml_configure (GstAmcCodec * codec, GstAmcFormat * format,
    GstAmcSurfaceTexture * surface_texture, GError ** err)
{
  MLResult result;
  MLHandle surface_handle = ML_INVALID_HANDLE;

  g_return_val_if_fail (codec != NULL, FALSE);
  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (surface_texture == NULL
      || GST_IS_AMC_SURFACE_TEXTURE_ML (surface_texture), FALSE);

  g_set_object (&codec->surface_texture, surface_texture);
  if (surface_texture != NULL)
    surface_handle =
        gst_amc_surface_texture_ml_get_handle ((GstAmcSurfaceTextureML *)
        surface_texture);

  result = MLMediaCodecConfigureWithSurface (codec->handle,
      gst_amc_format_ml_get_handle (format), surface_handle, 0);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to configure codec %d", result);
    return FALSE;
  }

  return TRUE;
}

static GstAmcFormat *
gst_amc_codec_ml_get_output_format (GstAmcCodec * codec, GError ** err)
{
  MLHandle format_handle;
  MLResult result;

  g_return_val_if_fail (codec != NULL, NULL);

  result = MLMediaCodecGetOutputFormat (codec->handle, &format_handle);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to get output format %d", result);
    return NULL;
  }

  return gst_amc_format_ml_new_handle (format_handle);
}

static gboolean
gst_amc_codec_ml_start (GstAmcCodec * codec, GError ** err)
{
  MLResult result;

  g_return_val_if_fail (codec != NULL, FALSE);

  result = MLMediaCodecStart (codec->handle);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to start codec %d", result);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_amc_codec_ml_stop (GstAmcCodec * codec, GError ** err)
{
  MLResult result;

  g_return_val_if_fail (codec != NULL, FALSE);

  result = MLMediaCodecStop (codec->handle);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to stop codec %d", result);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_amc_codec_ml_flush (GstAmcCodec * codec, GError ** err)
{
  MLResult result;

  g_return_val_if_fail (codec != NULL, FALSE);

  result = MLMediaCodecFlush (codec->handle);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to flush codec %d", result);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_amc_codec_ml_request_key_frame (GstAmcCodec * codec, GError ** err)
{
  /* If MagicLeap adds an API for requesting a keyframe, call it here */
  g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
      "Keyframe requests are not available on MagicLeap");
  return FALSE;
}

static gboolean
gst_amc_codec_ml_set_dynamic_bitrate (GstAmcCodec * codec, GError ** err,
    gint bitrate)
{
  g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
      "Dynamic bitrate control isn't available on MagicLeap");
  return FALSE;
}

static gboolean
gst_amc_codec_ml_have_dynamic_bitrate ()
{
  /* If MagicLeap ever provides an API for scaling bitrate, change this to TRUE */
  return FALSE;
}

static gboolean
gst_amc_codec_ml_release (GstAmcCodec * codec, GError ** err)
{
  g_return_val_if_fail (codec != NULL, FALSE);
  return TRUE;
}

static GstAmcBuffer *
gst_amc_codec_ml_get_output_buffer (GstAmcCodec * codec, gint index,
    GError ** err)
{
  MLResult result;
  GstAmcBuffer *ret;

  g_return_val_if_fail (codec != NULL, NULL);
  g_return_val_if_fail (index >= 0, NULL);

  ret = g_new0 (GstAmcBuffer, 1);

  /* When configured with a surface, getting the buffer pointer makes no sense,
   * but on Android it's not an error, it just return NULL buffer.
   * But MLMediaCodecGetInputBufferPointer() will return an error instead. */
  if (codec->surface_texture != NULL) {
    return ret;
  }

  result =
      MLMediaCodecGetOutputBufferPointer (codec->handle, index,
      (const uint8_t **) &ret->data, &ret->size);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to get output buffer %d", result);
    g_free (ret);
    return NULL;
  }

  return ret;
}

static GstAmcBuffer *
gst_amc_codec_ml_get_input_buffer (GstAmcCodec * codec, gint index,
    GError ** err)
{
  MLResult result;
  GstAmcBuffer *ret;

  g_return_val_if_fail (codec != NULL, NULL);
  g_return_val_if_fail (index >= 0, NULL);

  ret = g_new0 (GstAmcBuffer, 1);

  result =
      MLMediaCodecGetInputBufferPointer (codec->handle, index, &ret->data,
      &ret->size);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to get input buffer %d", result);
    g_free (ret);
    return NULL;
  }

  return ret;
}

static gint
gst_amc_codec_ml_dequeue_input_buffer (GstAmcCodec * codec, gint64 timeoutUs,
    GError ** err)
{
  MLResult result;
  int64_t index;

  g_return_val_if_fail (codec != NULL, G_MININT);

  result = MLMediaCodecDequeueInputBuffer (codec->handle, timeoutUs, &index);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to dequeue input buffer %d", result);
    return G_MININT;
  }

  if (index == MLMediaCodec_TryAgainLater)
    return INFO_TRY_AGAIN_LATER;

  return index;
}

static gint
gst_amc_codec_ml_dequeue_output_buffer (GstAmcCodec * codec,
    GstAmcBufferInfo * info, gint64 timeoutUs, GError ** err)
{
  MLMediaCodecBufferInfo info_;
  MLResult result;
  int64_t index;

  g_return_val_if_fail (codec != NULL, G_MININT);

  result =
      MLMediaCodecDequeueOutputBuffer (codec->handle, &info_, timeoutUs,
      &index);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to dequeue output buffer %d", result);
    return G_MININT;
  }

  if (index == MLMediaCodec_OutputBuffersChanged) {
    return gst_amc_codec_dequeue_output_buffer (codec, info, timeoutUs, err);
  } else if (index == MLMediaCodec_FormatChanged) {
    return INFO_OUTPUT_FORMAT_CHANGED;
  } else if (index == MLMediaCodec_TryAgainLater) {
    return INFO_TRY_AGAIN_LATER;
  }

  info->flags = info_.flags;

  info->offset = info_.offset;
  info->presentation_time_us = info_.presentation_time_us;
  info->size = info_.size;

  return index;
}

static gboolean
gst_amc_codec_ml_queue_input_buffer (GstAmcCodec * codec, gint index,
    const GstAmcBufferInfo * info, GError ** err)
{
  MLResult result;

  g_return_val_if_fail (codec != NULL, FALSE);
  g_return_val_if_fail (info != NULL, FALSE);

  result = MLMediaCodecQueueInputBuffer (codec->handle, index, info->offset,
      info->size, info->presentation_time_us, info->flags);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to queue input buffer %d", result);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_amc_codec_ml_release_output_buffer (GstAmcCodec * codec, gint index,
    gboolean render, GError ** err)
{
  MLResult result;

  g_return_val_if_fail (codec != NULL, FALSE);

  result = MLMediaCodecReleaseOutputBuffer (codec->handle, index, render);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to release output buffer %d", result);
    return FALSE;
  }

  return TRUE;
}

static GstAmcSurfaceTexture *
gst_amc_codec_ml_new_surface_texture (GError ** err)
{
  return (GstAmcSurfaceTexture *) gst_amc_surface_texture_ml_new (err);
}

GstAmcCodecVTable gst_amc_codec_ml_vtable = {
  .buffer_free = gst_amc_buffer_ml_free,
  .buffer_set_position_and_limit = gst_amc_buffer_ml_set_position_and_limit,

  .create = gst_amc_codec_ml_new,
  .free = gst_amc_codec_ml_free,

  .configure = gst_amc_codec_ml_configure,
  .get_output_format = gst_amc_codec_ml_get_output_format,

  .start = gst_amc_codec_ml_start,
  .stop = gst_amc_codec_ml_stop,
  .flush = gst_amc_codec_ml_flush,
  .request_key_frame = gst_amc_codec_ml_request_key_frame,

  .have_dynamic_bitrate = gst_amc_codec_ml_have_dynamic_bitrate,
  .set_dynamic_bitrate = gst_amc_codec_ml_set_dynamic_bitrate,

  .release = gst_amc_codec_ml_release,

  .get_output_buffer = gst_amc_codec_ml_get_output_buffer,
  .get_input_buffer = gst_amc_codec_ml_get_input_buffer,

  .dequeue_input_buffer = gst_amc_codec_ml_dequeue_input_buffer,
  .dequeue_output_buffer = gst_amc_codec_ml_dequeue_output_buffer,

  .queue_input_buffer = gst_amc_codec_ml_queue_input_buffer,
  .release_output_buffer = gst_amc_codec_ml_release_output_buffer,

  .new_surface_texture = gst_amc_codec_ml_new_surface_texture,
};
