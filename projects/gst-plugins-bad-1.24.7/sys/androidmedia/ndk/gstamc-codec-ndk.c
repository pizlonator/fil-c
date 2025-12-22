/*
 * Copyright (C) 2012,2018 Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2015, Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2023, Ratchanan Srirattanamet <peathot@hotmail.com>
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

#include "gstamc-ndk.h"
#include "gstamc-internal-ndk.h"
#include "../gstamc-codec.h"
#include "../gstamc-constants.h"

#include "gstjniutils.h"
#include "../jni/gstamcsurface.h"

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <media/NdkMediaError.h>
#include <media/NdkMediaCodec.h>

#include <dlfcn.h>

struct _GstAmcCodec
{
  AMediaCodec *ndk_media_codec;
  gboolean is_encoder;

  /* For JNI-based SurfaceTexture. */
  GstAmcSurface *surface;
};

/* The defines are from NdkMediaCodec.h. See the reasoning in the same file. */
#if defined(__USE_FILE_OFFSET64) && !defined(__LP64__)
#define _off_t_compat int32_t
#else
#define _off_t_compat off_t
#endif /* defined(__USE_FILE_OFFSET64) && !defined(__LP64__) */

static struct
{
  void *mediandk_handle;

  AMediaCodec *(*create_codec_by_name) (const char *name);
    media_status_t (*delete) (AMediaCodec *);

    media_status_t (*configure) (AMediaCodec *,
      const AMediaFormat * format,
      ANativeWindow * surface, AMediaCrypto * crypto, uint32_t flags);

    media_status_t (*start) (AMediaCodec *);
    media_status_t (*stop) (AMediaCodec *);
    media_status_t (*flush) (AMediaCodec *);

  uint8_t *(*get_input_buffer) (AMediaCodec *, size_t idx, size_t *out_size);
  uint8_t *(*get_output_buffer) (AMediaCodec *, size_t idx, size_t *out_size);
    ssize_t (*dequeue_input_buffer) (AMediaCodec *, int64_t timeoutUs);

    media_status_t (*queue_input_buffer) (AMediaCodec *,
      size_t idx,
      _off_t_compat offset, size_t size, uint64_t time, uint32_t flags);

    ssize_t (*dequeue_output_buffer) (AMediaCodec *,
      AMediaCodecBufferInfo * info, int64_t timeoutUs);

  AMediaFormat *(*get_output_format) (AMediaCodec *);

    media_status_t (*release_output_buffer) (AMediaCodec *,
      size_t idx, bool render);

  /* optional */
    media_status_t (*set_parameters) (AMediaCodec * mData,
      const AMediaFormat * params);
} a_media_codec;

#undef _off_t_compat

gboolean
gst_amc_codec_ndk_static_init (void)
{
  a_media_codec.mediandk_handle = dlopen ("libmediandk.so", RTLD_NOW);

  if (!a_media_codec.mediandk_handle)
    return FALSE;

  a_media_codec.create_codec_by_name =
      dlsym (a_media_codec.mediandk_handle, "AMediaCodec_createCodecByName");
  a_media_codec.delete =
      dlsym (a_media_codec.mediandk_handle, "AMediaCodec_delete");
  a_media_codec.configure =
      dlsym (a_media_codec.mediandk_handle, "AMediaCodec_configure");
  a_media_codec.start =
      dlsym (a_media_codec.mediandk_handle, "AMediaCodec_start");
  a_media_codec.stop =
      dlsym (a_media_codec.mediandk_handle, "AMediaCodec_stop");
  a_media_codec.flush =
      dlsym (a_media_codec.mediandk_handle, "AMediaCodec_flush");
  a_media_codec.get_input_buffer =
      dlsym (a_media_codec.mediandk_handle, "AMediaCodec_getInputBuffer");
  a_media_codec.get_output_buffer =
      dlsym (a_media_codec.mediandk_handle, "AMediaCodec_getOutputBuffer");
  a_media_codec.dequeue_input_buffer =
      dlsym (a_media_codec.mediandk_handle, "AMediaCodec_dequeueInputBuffer");
  a_media_codec.queue_input_buffer =
      dlsym (a_media_codec.mediandk_handle, "AMediaCodec_queueInputBuffer");
  a_media_codec.dequeue_output_buffer =
      dlsym (a_media_codec.mediandk_handle, "AMediaCodec_dequeueOutputBuffer");
  a_media_codec.get_output_format =
      dlsym (a_media_codec.mediandk_handle, "AMediaCodec_getOutputFormat");
  a_media_codec.release_output_buffer =
      dlsym (a_media_codec.mediandk_handle, "AMediaCodec_releaseOutputBuffer");

  if (!a_media_codec.create_codec_by_name || !a_media_codec.delete
      || !a_media_codec.configure || !a_media_codec.start || !a_media_codec.stop
      || !a_media_codec.flush || !a_media_codec.get_input_buffer
      || !a_media_codec.get_output_buffer || !a_media_codec.dequeue_input_buffer
      || !a_media_codec.queue_input_buffer
      || !a_media_codec.dequeue_output_buffer
      || !a_media_codec.get_output_format
      || !a_media_codec.release_output_buffer) {
    GST_WARNING ("Failed to get AMediaCodec functions");
    dlclose (a_media_codec.mediandk_handle);
    return FALSE;
  }

  /* Optional. */
  a_media_codec.set_parameters =
      dlsym (a_media_codec.mediandk_handle, "AMediaCodec_setParameters");

  return TRUE;
}

static void
gst_amc_buffer_ndk_free (GstAmcBuffer * buffer)
{
  g_free (buffer);
}

static gboolean
gst_amc_buffer_ndk_set_position_and_limit (GstAmcBuffer * buffer_,
    GError ** err, gint position, gint limit)
{
/* FIXME: Do we need to do something?
  buffer->data = buffer->data + position;
  buffer->size = limit;
*/
  return TRUE;
}

static GstAmcCodec *
gst_amc_codec_ndk_new (const gchar * name, gboolean is_encoder, GError ** err)
{
  GstAmcCodec *codec = NULL;

  g_return_val_if_fail (name != NULL, NULL);

  codec = g_new0 (GstAmcCodec, 1);
  codec->ndk_media_codec = a_media_codec.create_codec_by_name (name);
  codec->is_encoder = is_encoder;

  if (!codec->ndk_media_codec) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to create codec by name %s", name);
    g_free (codec);
    return NULL;
  }

  return codec;
}

static void
gst_amc_codec_ndk_free (GstAmcCodec * codec)
{
  media_status_t result;

  result = a_media_codec.delete (codec->ndk_media_codec);

  if (result != AMEDIA_OK) {
    GST_WARNING
        ("Unable to delete an AMediaCodec: %d, a leak might have occured.",
        result);
  }

  if (codec->surface)
    g_object_unref (codec->surface);

  g_free (codec);
}

static gboolean
gst_amc_codec_ndk_configure (GstAmcCodec * codec, GstAmcFormat * format,
    GstAmcSurfaceTexture * surface_texture, GError ** err)
{
  gboolean ret;
  media_status_t result;
  ANativeWindow *native_window = NULL;
  uint32_t flags = 0;

  g_return_val_if_fail (codec != NULL, FALSE);
  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (surface_texture == NULL
      || GST_IS_AMC_SURFACE_TEXTURE_JNI (surface_texture), FALSE);

  if (surface_texture) {
    if (codec->surface)
      g_object_unref (codec->surface);

    if (GST_IS_AMC_SURFACE_TEXTURE_JNI (surface_texture)) {
      JNIEnv *env;

      codec->surface = gst_amc_surface_new (
          (GstAmcSurfaceTextureJNI *) surface_texture, err);
      if (!codec->surface)
        return FALSE;

      env = gst_amc_jni_get_env ();
      native_window = ANativeWindow_fromSurface (env, codec->surface->jobject);

      if (!native_window)
        return FALSE;
      /* TODO: support NDK-based ASurfaceTexture. */
    } else {
      g_assert_not_reached ();
    }
  }

  if (codec->is_encoder)
    flags = AMEDIACODEC_CONFIGURE_FLAG_ENCODE;

  result = a_media_codec.configure (codec->ndk_media_codec,
      format->ndk_media_format, native_window, NULL, flags);

  if (result != AMEDIA_OK) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to configure codec: %d", result);
    ret = FALSE;
    goto out;
  }

  ret = TRUE;

out:
  if (native_window)
    ANativeWindow_release (native_window);

  return ret;
}

static GstAmcFormat *
gst_amc_codec_ndk_get_output_format (GstAmcCodec * codec, GError ** err)
{
  AMediaFormat *ndk_media_format;

  g_return_val_if_fail (codec != NULL, NULL);

  ndk_media_format = a_media_codec.get_output_format (codec->ndk_media_codec);
  if (!ndk_media_format) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to get output format");
    return NULL;
  }

  return gst_amc_format_ndk_from_a_media_format (ndk_media_format);
}

static gboolean
gst_amc_codec_ndk_start (GstAmcCodec * codec, GError ** err)
{
  media_status_t result;

  g_return_val_if_fail (codec != NULL, FALSE);

  result = a_media_codec.start (codec->ndk_media_codec);

  if (result != AMEDIA_OK) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to start codec: %d", result);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_amc_codec_ndk_stop (GstAmcCodec * codec, GError ** err)
{
  media_status_t result;

  g_return_val_if_fail (codec != NULL, FALSE);

  result = a_media_codec.stop (codec->ndk_media_codec);

  if (result != AMEDIA_OK) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to stop codec: %d", result);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_amc_codec_ndk_flush (GstAmcCodec * codec, GError ** err)
{
  media_status_t result;

  g_return_val_if_fail (codec != NULL, FALSE);

  result = a_media_codec.flush (codec->ndk_media_codec);

  if (result != AMEDIA_OK) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to flush codec: %d", result);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_amc_codec_ndk_set_parameter (GstAmcCodec * codec, GError ** err,
    const gchar * key, int value)
{
  GstAmcFormat *format;
  media_status_t result;
  gboolean ret = TRUE;

  if (!a_media_codec.set_parameters) {
    /* Not available means we're on Android < 26
     * Note: this is a degradation in feature compared to JNI counterpart. */
    return FALSE;
  }

  format = gst_amc_format_ndk_new (err);
  if (!format)
    return FALSE;

  if (!gst_amc_format_set_int (format, key, value, err)) {
    ret = FALSE;
    goto done;
  }

  result =
      a_media_codec.set_parameters (codec->ndk_media_codec,
      format->ndk_media_format);

  if (result != AMEDIA_OK) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to set a parameter: %d", result);
    ret = FALSE;
  }

done:
  gst_amc_format_free (format);

  return ret;
}

#define PARAMETER_KEY_REQUEST_SYNC_FRAME "request-sync"

static gboolean
gst_amc_codec_ndk_request_key_frame (GstAmcCodec * codec, GError ** err)
{
  g_return_val_if_fail (codec != NULL, FALSE);

  return gst_amc_codec_ndk_set_parameter (codec, err,
      PARAMETER_KEY_REQUEST_SYNC_FRAME, 0);
}

static gboolean
gst_amc_codec_ndk_have_dynamic_bitrate ()
{
  /* Dynamic bitrate scaling is supported on Android >= 26,
   * where the setParameters() call is available
   * Note: this is a degradation in feature compared to JNI counterpart. */
  return (a_media_codec.set_parameters != NULL);
}

#define PARAMETER_KEY_VIDEO_BITRATE "video-bitrate"

static gboolean
gst_amc_codec_ndk_set_dynamic_bitrate (GstAmcCodec * codec, GError ** err,
    gint bitrate)
{
  g_return_val_if_fail (codec != NULL, FALSE);

  return gst_amc_codec_ndk_set_parameter (codec, err,
      PARAMETER_KEY_VIDEO_BITRATE, bitrate);
}

static gboolean
gst_amc_codec_ndk_release (GstAmcCodec * codec, GError ** err)
{
  g_return_val_if_fail (codec != NULL, FALSE);

  /* Do nothing. AMediaCodec_delete() already covers this. */
  return TRUE;
}

static GstAmcBuffer *
gst_amc_codec_ndk_get_output_buffer (GstAmcCodec * codec, gint index,
    GError ** err)
{
  GstAmcBuffer *ret;

  g_return_val_if_fail (codec != NULL, NULL);

  ret = g_new0 (GstAmcBuffer, 1);
  ret->data = a_media_codec.get_output_buffer (codec->ndk_media_codec,
      index, &ret->size);

  if (!ret->data) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to get output buffer for idx %d.", index);
    g_free (ret);
    return NULL;
  }

  return ret;
}

static GstAmcBuffer *
gst_amc_codec_ndk_get_input_buffer (GstAmcCodec * codec, gint index,
    GError ** err)
{
  GstAmcBuffer *ret;

  g_return_val_if_fail (codec != NULL, NULL);

  ret = g_new0 (GstAmcBuffer, 1);
  ret->data = a_media_codec.get_input_buffer (codec->ndk_media_codec,
      index, &ret->size);

  if (!ret->data) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to get input buffer for idx %d.", index);
    g_free (ret);
    return NULL;
  }

  return ret;
}

static gint
gst_amc_codec_ndk_dequeue_input_buffer (GstAmcCodec * codec, gint64 timeoutUs,
    GError ** err)
{
  gint ret;

  g_return_val_if_fail (codec != NULL, G_MININT);

  ret = a_media_codec.dequeue_input_buffer (codec->ndk_media_codec, timeoutUs);

  /* AMediaCodec's error code is the same as Java's MediaCodec, thus no
   * translation is required. */

  return ret;
}

static gint
gst_amc_codec_ndk_dequeue_output_buffer (GstAmcCodec * codec,
    GstAmcBufferInfo * info, gint64 timeoutUs, GError ** err)
{
  gint ret;
  AMediaCodecBufferInfo a_info;

  g_return_val_if_fail (codec != NULL, G_MININT);

  ret = a_media_codec.dequeue_output_buffer (codec->ndk_media_codec,
      &a_info, timeoutUs);

  if (ret == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
    return gst_amc_codec_ndk_dequeue_output_buffer (codec, info, timeoutUs,
        err);
  } else if (ret < 0) {
    /* AMediaCodec's error code is the same as Java's MediaCodec, thus no
     * translation is required. */
    return ret;
  }

  info->flags = a_info.flags;
  info->offset = a_info.offset;
  info->presentation_time_us = a_info.presentationTimeUs;
  info->size = a_info.size;

  return ret;
}

static gboolean
gst_amc_codec_ndk_queue_input_buffer (GstAmcCodec * codec, gint index,
    const GstAmcBufferInfo * info, GError ** err)
{
  media_status_t result;

  g_return_val_if_fail (codec != NULL, FALSE);

  result = a_media_codec.queue_input_buffer (codec->ndk_media_codec,
      index, info->offset, info->size, info->presentation_time_us, info->flags);

  if (result != AMEDIA_OK) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to queue input buffer: %d", result);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_amc_codec_ndk_release_output_buffer (GstAmcCodec * codec, gint index,
    gboolean render, GError ** err)
{
  media_status_t result;

  g_return_val_if_fail (codec != NULL, FALSE);

  result = a_media_codec.release_output_buffer (codec->ndk_media_codec,
      index, render);

  if (result != AMEDIA_OK) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to release input buffer: %d", result);
    return FALSE;
  }

  return TRUE;
}

static GstAmcSurfaceTexture *
gst_amc_codec_ndk_new_surface_texture (GError ** err)
{
  /* TODO: support NDK-based ASurfaceTexture. */
  return (GstAmcSurfaceTexture *) gst_amc_surface_texture_jni_new (err);
}

GstAmcCodecVTable gst_amc_codec_ndk_vtable = {
  .buffer_free = gst_amc_buffer_ndk_free,
  .buffer_set_position_and_limit = gst_amc_buffer_ndk_set_position_and_limit,

  .create = gst_amc_codec_ndk_new,
  .free = gst_amc_codec_ndk_free,

  .configure = gst_amc_codec_ndk_configure,
  .get_output_format = gst_amc_codec_ndk_get_output_format,

  .start = gst_amc_codec_ndk_start,
  .stop = gst_amc_codec_ndk_stop,
  .flush = gst_amc_codec_ndk_flush,
  .request_key_frame = gst_amc_codec_ndk_request_key_frame,

  .have_dynamic_bitrate = gst_amc_codec_ndk_have_dynamic_bitrate,
  .set_dynamic_bitrate = gst_amc_codec_ndk_set_dynamic_bitrate,

  .release = gst_amc_codec_ndk_release,

  .get_output_buffer = gst_amc_codec_ndk_get_output_buffer,
  .get_input_buffer = gst_amc_codec_ndk_get_input_buffer,

  .dequeue_input_buffer = gst_amc_codec_ndk_dequeue_input_buffer,
  .dequeue_output_buffer = gst_amc_codec_ndk_dequeue_output_buffer,

  .queue_input_buffer = gst_amc_codec_ndk_queue_input_buffer,
  .release_output_buffer = gst_amc_codec_ndk_release_output_buffer,

  .new_surface_texture = gst_amc_codec_ndk_new_surface_texture,
};
