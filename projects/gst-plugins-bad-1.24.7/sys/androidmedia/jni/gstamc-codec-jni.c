/*
 * Copyright (C) 2012,2018 Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2015, Sebastian Dröge <sebastian@centricular.com>
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

#include "../gstjniutils.h"
#include "../gstamc-codec.h"
#include "../gstamc-constants.h"
#include "gstamc-jni.h"
#include "gstamc-internal-jni.h"
#include "gstamcsurfacetexture-jni.h"
#include "gstamcsurface.h"

#define PARAMETER_KEY_REQUEST_SYNC_FRAME "request-sync"
#define PARAMETER_KEY_VIDEO_BITRATE "video-bitrate"

struct _GstAmcCodec
{
  jobject object;               /* global reference */

  RealBuffer *input_buffers, *output_buffers;
  gsize n_input_buffers, n_output_buffers;
  GstAmcSurface *surface;
  gboolean is_encoder;
};

static struct
{
  jclass klass;
  jmethodID configure;
  jmethodID create_by_codec_name;
  jmethodID dequeue_input_buffer;
  jmethodID dequeue_output_buffer;
  jmethodID flush;
  jmethodID get_input_buffers;
  jmethodID get_input_buffer;
  jmethodID get_output_buffers;
  jmethodID get_output_buffer;
  jmethodID get_output_format;
  jmethodID queue_input_buffer;
  jmethodID release;
  jmethodID release_output_buffer;
  jmethodID start;
  jmethodID stop;
  jmethodID setParameters;
} media_codec;

static struct
{
  jclass klass;
  jmethodID constructor;
  jfieldID flags;
  jfieldID offset;
  jfieldID presentation_time_us;
  jfieldID size;
} media_codec_buffer_info;

static struct
{
  jclass klass;
  jmethodID constructor;
  jmethodID putInt;
} bundle_class;

static struct
{
  jclass klass;
  jmethodID get_limit, get_position;
  jmethodID set_limit, set_position;
  jmethodID clear;
} java_nio_buffer;

gboolean
gst_amc_codec_jni_static_init (void)
{
  gboolean ret = TRUE;
  JNIEnv *env;
  jclass tmp;
  GError *err = NULL;

  env = gst_amc_jni_get_env ();

  java_nio_buffer.klass = gst_amc_jni_get_class (env, &err, "java/nio/Buffer");
  if (!java_nio_buffer.klass) {
    GST_ERROR ("Failed to get java.nio.Buffer class: %s", err->message);
    g_clear_error (&err);
    return FALSE;
  }

  java_nio_buffer.get_limit =
      gst_amc_jni_get_method_id (env, &err, java_nio_buffer.klass, "limit",
      "()I");
  if (!java_nio_buffer.get_limit) {
    GST_ERROR ("Failed to get java.nio.Buffer limit(): %s", err->message);
    g_clear_error (&err);
    return FALSE;
  }

  java_nio_buffer.get_position =
      gst_amc_jni_get_method_id (env, &err, java_nio_buffer.klass, "position",
      "()I");
  if (!java_nio_buffer.get_position) {
    GST_ERROR ("Failed to get java.nio.Buffer position(): %s", err->message);
    g_clear_error (&err);
    return FALSE;
  }

  java_nio_buffer.set_limit =
      gst_amc_jni_get_method_id (env, &err, java_nio_buffer.klass, "limit",
      "(I)Ljava/nio/Buffer;");
  if (!java_nio_buffer.set_limit) {
    GST_ERROR ("Failed to get java.nio.Buffer limit(): %s", err->message);
    g_clear_error (&err);
    return FALSE;
  }

  java_nio_buffer.set_position =
      gst_amc_jni_get_method_id (env, &err, java_nio_buffer.klass, "position",
      "(I)Ljava/nio/Buffer;");
  if (!java_nio_buffer.set_position) {
    GST_ERROR ("Failed to get java.nio.Buffer position(): %s", err->message);
    g_clear_error (&err);
    return FALSE;
  }

  java_nio_buffer.clear =
      gst_amc_jni_get_method_id (env, &err, java_nio_buffer.klass, "clear",
      "()Ljava/nio/Buffer;");
  if (!java_nio_buffer.clear) {
    GST_ERROR ("Failed to get java.nio.Buffer clear(): %s", err->message);
    g_clear_error (&err);
    return FALSE;
  }

  tmp = (*env)->FindClass (env, "android/media/MediaCodec$BufferInfo");
  if (!tmp) {
    ret = FALSE;
    (*env)->ExceptionClear (env);
    GST_ERROR ("Failed to get codec buffer info class");
    goto done;
  }
  media_codec_buffer_info.klass = (*env)->NewGlobalRef (env, tmp);
  if (!media_codec_buffer_info.klass) {
    ret = FALSE;
    GST_ERROR ("Failed to get codec buffer info class global reference");
    if ((*env)->ExceptionCheck (env)) {
      (*env)->ExceptionDescribe (env);
      (*env)->ExceptionClear (env);
    }
    goto done;
  }
  (*env)->DeleteLocalRef (env, tmp);
  tmp = NULL;

  media_codec_buffer_info.constructor =
      (*env)->GetMethodID (env, media_codec_buffer_info.klass, "<init>", "()V");
  media_codec_buffer_info.flags =
      (*env)->GetFieldID (env, media_codec_buffer_info.klass, "flags", "I");
  media_codec_buffer_info.offset =
      (*env)->GetFieldID (env, media_codec_buffer_info.klass, "offset", "I");
  media_codec_buffer_info.presentation_time_us =
      (*env)->GetFieldID (env, media_codec_buffer_info.klass,
      "presentationTimeUs", "J");
  media_codec_buffer_info.size =
      (*env)->GetFieldID (env, media_codec_buffer_info.klass, "size", "I");
  if (!media_codec_buffer_info.constructor || !media_codec_buffer_info.flags
      || !media_codec_buffer_info.offset
      || !media_codec_buffer_info.presentation_time_us
      || !media_codec_buffer_info.size) {
    ret = FALSE;
    GST_ERROR ("Failed to get buffer info methods and fields");
    if ((*env)->ExceptionCheck (env)) {
      (*env)->ExceptionDescribe (env);
      (*env)->ExceptionClear (env);
    }
    goto done;
  }

  tmp = (*env)->FindClass (env, "android/media/MediaCodec");
  if (!tmp) {
    ret = FALSE;
    GST_ERROR ("Failed to get codec class");
    if ((*env)->ExceptionCheck (env)) {
      (*env)->ExceptionDescribe (env);
      (*env)->ExceptionClear (env);
    }
    goto done;
  }
  media_codec.klass = (*env)->NewGlobalRef (env, tmp);
  if (!media_codec.klass) {
    ret = FALSE;
    GST_ERROR ("Failed to get codec class global reference");
    if ((*env)->ExceptionCheck (env)) {
      (*env)->ExceptionDescribe (env);
      (*env)->ExceptionClear (env);
    }
    goto done;
  }
  (*env)->DeleteLocalRef (env, tmp);
  tmp = NULL;

  media_codec.create_by_codec_name =
      (*env)->GetStaticMethodID (env, media_codec.klass, "createByCodecName",
      "(Ljava/lang/String;)Landroid/media/MediaCodec;");
  media_codec.configure =
      (*env)->GetMethodID (env, media_codec.klass, "configure",
      "(Landroid/media/MediaFormat;Landroid/view/Surface;Landroid/media/MediaCrypto;I)V");
  media_codec.dequeue_input_buffer =
      (*env)->GetMethodID (env, media_codec.klass, "dequeueInputBuffer",
      "(J)I");
  media_codec.dequeue_output_buffer =
      (*env)->GetMethodID (env, media_codec.klass, "dequeueOutputBuffer",
      "(Landroid/media/MediaCodec$BufferInfo;J)I");
  media_codec.flush =
      (*env)->GetMethodID (env, media_codec.klass, "flush", "()V");
  media_codec.get_input_buffers =
      (*env)->GetMethodID (env, media_codec.klass, "getInputBuffers",
      "()[Ljava/nio/ByteBuffer;");
  media_codec.get_output_buffers =
      (*env)->GetMethodID (env, media_codec.klass, "getOutputBuffers",
      "()[Ljava/nio/ByteBuffer;");
  media_codec.get_output_format =
      (*env)->GetMethodID (env, media_codec.klass, "getOutputFormat",
      "()Landroid/media/MediaFormat;");
  media_codec.queue_input_buffer =
      (*env)->GetMethodID (env, media_codec.klass, "queueInputBuffer",
      "(IIIJI)V");
  media_codec.release =
      (*env)->GetMethodID (env, media_codec.klass, "release", "()V");
  media_codec.release_output_buffer =
      (*env)->GetMethodID (env, media_codec.klass, "releaseOutputBuffer",
      "(IZ)V");
  media_codec.start =
      (*env)->GetMethodID (env, media_codec.klass, "start", "()V");
  media_codec.stop =
      (*env)->GetMethodID (env, media_codec.klass, "stop", "()V");

  if (!media_codec.configure ||
      !media_codec.create_by_codec_name ||
      !media_codec.dequeue_input_buffer ||
      !media_codec.dequeue_output_buffer ||
      !media_codec.flush ||
      !media_codec.get_input_buffers ||
      !media_codec.get_output_buffers ||
      !media_codec.get_output_format ||
      !media_codec.queue_input_buffer ||
      !media_codec.release ||
      !media_codec.release_output_buffer ||
      !media_codec.start || !media_codec.stop) {
    ret = FALSE;
    GST_ERROR ("Failed to get codec methods");
    if ((*env)->ExceptionCheck (env)) {
      (*env)->ExceptionDescribe (env);
      (*env)->ExceptionClear (env);
    }
    goto done;
  }
  media_codec.setParameters =
      (*env)->GetMethodID (env, media_codec.klass, "setParameters",
      "(Landroid/os/Bundle;)V");
  if ((*env)->ExceptionCheck (env))
    (*env)->ExceptionClear (env);

  /* Android >= 21 */
  media_codec.get_output_buffer =
      (*env)->GetMethodID (env, media_codec.klass, "getOutputBuffer",
      "(I)Ljava/nio/ByteBuffer;");
  if ((*env)->ExceptionCheck (env))
    (*env)->ExceptionClear (env);

  /* Android >= 21 */
  media_codec.get_input_buffer =
      (*env)->GetMethodID (env, media_codec.klass, "getInputBuffer",
      "(I)Ljava/nio/ByteBuffer;");
  if ((*env)->ExceptionCheck (env))
    (*env)->ExceptionClear (env);

  if (media_codec.setParameters != NULL) {
    /* Bundle needed for parameter setting on Android >= 19 */
    tmp = (*env)->FindClass (env, "android/os/Bundle");
    if (!tmp) {
      ret = FALSE;
      GST_ERROR ("Failed to get Bundle class");
      if ((*env)->ExceptionCheck (env)) {
        (*env)->ExceptionDescribe (env);
        (*env)->ExceptionClear (env);
      }
      goto done;
    }
    bundle_class.klass = (*env)->NewGlobalRef (env, tmp);
    if (!bundle_class.klass) {
      ret = FALSE;
      GST_ERROR ("Failed to get Bundle class global reference");
      if ((*env)->ExceptionCheck (env)) {
        (*env)->ExceptionDescribe (env);
        (*env)->ExceptionClear (env);
      }
      goto done;
    }
    (*env)->DeleteLocalRef (env, tmp);
    tmp = NULL;

    bundle_class.constructor =
        (*env)->GetMethodID (env, bundle_class.klass, "<init>", "()V");
    bundle_class.putInt =
        (*env)->GetMethodID (env, bundle_class.klass, "putInt",
        "(Ljava/lang/String;I)V");
    if (!bundle_class.constructor || !bundle_class.putInt) {
      ret = FALSE;
      GST_ERROR ("Failed to get Bundle methods");
      if ((*env)->ExceptionCheck (env)) {
        (*env)->ExceptionDescribe (env);
        (*env)->ExceptionClear (env);
      }
      goto done;
    }
  }

done:
  if (tmp)
    (*env)->DeleteLocalRef (env, tmp);
  tmp = NULL;

  return ret;
}

static void
gst_amc_jni_free_buffer_array (JNIEnv * env, RealBuffer * buffers,
    gsize n_buffers)
{
  jsize i;

  g_return_if_fail (buffers != NULL);

  for (i = 0; i < n_buffers; i++) {
    if (buffers[i].object)
      gst_amc_jni_object_unref (env, buffers[i].object);
  }
  g_free (buffers);
}

static gboolean
gst_amc_jni_get_buffer_array (JNIEnv * env, GError ** err, jobject array,
    RealBuffer ** buffers_, gsize * n_buffers)
{
  RealBuffer **buffers = (RealBuffer **) buffers_;
  jsize i;

  *n_buffers = (*env)->GetArrayLength (env, array);
  *buffers = g_new0 (RealBuffer, *n_buffers);

  for (i = 0; i < *n_buffers; i++) {
    jobject buffer = NULL;

    buffer = (*env)->GetObjectArrayElement (env, array, i);
    if ((*env)->ExceptionCheck (env)) {
      gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
          GST_LIBRARY_ERROR_FAILED, "Failed to get buffer %d", i);
      goto error;
    }

    /* NULL buffers are not a problem and are happening when we configured
     * a surface as input/output */
    if (!buffer)
      continue;

    (*buffers)[i].object = gst_amc_jni_object_make_global (env, buffer);
    if (!(*buffers)[i].object) {
      gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
          GST_LIBRARY_ERROR_FAILED,
          "Failed to create global buffer reference %d", i);
      goto error;
    }

    (*buffers)[i].data =
        (*env)->GetDirectBufferAddress (env, (*buffers)[i].object);
    if (!(*buffers)[i].data) {
      gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
          GST_LIBRARY_ERROR_FAILED, "Failed to get buffer address %d", i);
      goto error;
    }
    (*buffers)[i].size =
        (*env)->GetDirectBufferCapacity (env, (*buffers)[i].object);
  }

  return TRUE;

error:
  if (*buffers)
    gst_amc_jni_free_buffer_array (env, *buffers, *n_buffers);
  *buffers = NULL;
  *n_buffers = 0;
  return FALSE;
}

static void
gst_amc_buffer_jni_free (GstAmcBuffer * buffer_)
{
  RealBuffer *buffer = (RealBuffer *) buffer_;
  JNIEnv *env;

  g_return_if_fail (buffer != NULL);

  env = gst_amc_jni_get_env ();

  if (buffer->object)
    gst_amc_jni_object_unref (env, buffer->object);
  g_free (buffer);
}

static GstAmcBuffer *
gst_amc_buffer_copy (RealBuffer * buffer)
{
  JNIEnv *env;
  RealBuffer *ret;

  g_return_val_if_fail (buffer != NULL, NULL);

  env = gst_amc_jni_get_env ();

  ret = g_new0 (RealBuffer, 1);

  ret->object = gst_amc_jni_object_ref (env, buffer->object);
  ret->data = buffer->data;
  ret->size = buffer->size;

  return (GstAmcBuffer *) ret;
}

gboolean
gst_amc_buffer_get_position_and_limit (RealBuffer * buffer_, GError ** err,
    gint * position, gint * limit)
{
  RealBuffer *buffer = (RealBuffer *) buffer_;
  JNIEnv *env;

  g_return_val_if_fail (buffer != NULL, FALSE);
  g_return_val_if_fail (buffer->object != NULL, FALSE);

  env = gst_amc_jni_get_env ();

  if (!gst_amc_jni_call_int_method (env, err, buffer->object,
          java_nio_buffer.get_position, position))
    return FALSE;

  if (!gst_amc_jni_call_int_method (env, err, buffer->object,
          java_nio_buffer.get_limit, limit))
    return FALSE;

  return TRUE;
}

static gboolean
gst_amc_buffer_jni_set_position_and_limit (GstAmcBuffer * buffer_,
    GError ** err, gint position, gint limit)
{
  RealBuffer *buffer = (RealBuffer *) buffer_;
  JNIEnv *env;
  jobject tmp;

  g_return_val_if_fail (buffer != NULL, FALSE);
  g_return_val_if_fail (buffer->object != NULL, FALSE);

  env = gst_amc_jni_get_env ();

  if (!gst_amc_jni_call_object_method (env, err, buffer->object,
          java_nio_buffer.set_limit, &tmp, limit))
    return FALSE;

  gst_amc_jni_object_local_unref (env, tmp);

  if (!gst_amc_jni_call_object_method (env, err, buffer->object,
          java_nio_buffer.set_position, &tmp, position))
    return FALSE;

  gst_amc_jni_object_local_unref (env, tmp);

  return TRUE;
}

static GstAmcCodec *
gst_amc_codec_jni_new (const gchar * name, gboolean is_encoder, GError ** err)
{
  JNIEnv *env;
  GstAmcCodec *codec = NULL;
  jstring name_str;
  jobject object = NULL;

  g_return_val_if_fail (name != NULL, NULL);

  env = gst_amc_jni_get_env ();

  name_str = gst_amc_jni_string_from_gchar (env, err, FALSE, name);
  if (!name_str) {
    goto error;
  }

  codec = g_slice_new0 (GstAmcCodec);
  codec->is_encoder = is_encoder;

  if (!gst_amc_jni_call_static_object_method (env, err, media_codec.klass,
          media_codec.create_by_codec_name, &object, name_str))
    goto error;

  codec->object = gst_amc_jni_object_make_global (env, object);
  object = NULL;

  if (!codec->object) {
    gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
        GST_LIBRARY_ERROR_SETTINGS, "Failed to create global codec reference");
    goto error;
  }

done:
  if (name_str)
    gst_amc_jni_object_local_unref (env, name_str);
  name_str = NULL;

  return codec;

error:
  if (codec)
    g_slice_free (GstAmcCodec, codec);
  codec = NULL;
  goto done;
}

static void
gst_amc_codec_jni_free (GstAmcCodec * codec)
{
  JNIEnv *env;

  g_return_if_fail (codec != NULL);

  env = gst_amc_jni_get_env ();

  if (codec->input_buffers)
    gst_amc_jni_free_buffer_array (env, codec->input_buffers,
        codec->n_input_buffers);
  codec->input_buffers = NULL;
  codec->n_input_buffers = 0;

  if (codec->output_buffers)
    gst_amc_jni_free_buffer_array (env, codec->output_buffers,
        codec->n_output_buffers);
  codec->output_buffers = NULL;
  codec->n_output_buffers = 0;

  g_clear_object (&codec->surface);

  gst_amc_jni_object_unref (env, codec->object);
  g_slice_free (GstAmcCodec, codec);
}

static gboolean
gst_amc_codec_jni_configure (GstAmcCodec * codec, GstAmcFormat * format,
    GstAmcSurfaceTexture * surface, GError ** err)
{
  JNIEnv *env;
  gint flags = 0;

  g_return_val_if_fail (codec != NULL, FALSE);
  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (surface == NULL
      || GST_IS_AMC_SURFACE_TEXTURE_JNI (surface), FALSE);

  env = gst_amc_jni_get_env ();

  if (surface) {
    g_object_unref (codec->surface);
    codec->surface =
        gst_amc_surface_new ((GstAmcSurfaceTextureJNI *) surface, err);
    if (!codec->surface)
      return FALSE;
  }

  if (codec->is_encoder)
    flags = 1;

  return gst_amc_jni_call_void_method (env, err, codec->object,
      media_codec.configure, format->object,
      codec->surface ? codec->surface->jobject : NULL, NULL, flags);
}

static GstAmcFormat *
gst_amc_codec_jni_get_output_format (GstAmcCodec * codec, GError ** err)
{
  JNIEnv *env;
  GstAmcFormat *ret = NULL;
  jobject object = NULL;

  g_return_val_if_fail (codec != NULL, NULL);

  env = gst_amc_jni_get_env ();

  if (!gst_amc_jni_call_object_method (env, err, codec->object,
          media_codec.get_output_format, &object))
    goto done;

  ret = g_slice_new0 (GstAmcFormat);

  ret->object = gst_amc_jni_object_make_global (env, object);
  if (!ret->object) {
    gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
        GST_LIBRARY_ERROR_SETTINGS, "Failed to create global format reference");
    g_slice_free (GstAmcFormat, ret);
    ret = NULL;
  }

done:

  return ret;
}

static RealBuffer *
gst_amc_codec_jni_get_input_buffers (GstAmcCodec * codec,
    gsize * n_buffers, GError ** err)
{
  JNIEnv *env;
  jobject input_buffers = NULL;
  RealBuffer *ret = NULL;

  g_return_val_if_fail (codec != NULL, NULL);
  g_return_val_if_fail (n_buffers != NULL, NULL);

  *n_buffers = 0;
  env = gst_amc_jni_get_env ();

  if (!gst_amc_jni_call_object_method (env, err, codec->object,
          media_codec.get_input_buffers, &input_buffers))
    goto done;

  gst_amc_jni_get_buffer_array (env, err, input_buffers, &ret, n_buffers);

done:
  if (input_buffers)
    gst_amc_jni_object_local_unref (env, input_buffers);

  return ret;
}

static RealBuffer *
gst_amc_codec_jni_get_output_buffers (GstAmcCodec * codec,
    gsize * n_buffers, GError ** err)
{
  JNIEnv *env;
  jobject output_buffers = NULL;
  RealBuffer *ret = NULL;

  g_return_val_if_fail (codec != NULL, NULL);
  g_return_val_if_fail (n_buffers != NULL, NULL);

  *n_buffers = 0;
  env = gst_amc_jni_get_env ();

  if (!gst_amc_jni_call_object_method (env, err, codec->object,
          media_codec.get_output_buffers, &output_buffers))
    goto done;

  gst_amc_jni_get_buffer_array (env, err, output_buffers, &ret, n_buffers);

done:
  if (output_buffers)
    gst_amc_jni_object_local_unref (env, output_buffers);

  return ret;
}

static gboolean
gst_amc_codec_jni_start (GstAmcCodec * codec, GError ** err)
{
  JNIEnv *env;
  gboolean ret;

  g_return_val_if_fail (codec != NULL, FALSE);

  env = gst_amc_jni_get_env ();
  ret = gst_amc_jni_call_void_method (env, err, codec->object,
      media_codec.start);
  if (!ret)
    return ret;

  if (!media_codec.get_input_buffer) {
    if (codec->input_buffers)
      gst_amc_jni_free_buffer_array (env, codec->input_buffers,
          codec->n_input_buffers);
    codec->input_buffers =
        gst_amc_codec_jni_get_input_buffers (codec, &codec->n_input_buffers,
        err);
    if (!codec->input_buffers) {
      gst_amc_codec_stop (codec, NULL);
      return FALSE;
    }
  }

  return ret;
}

static gboolean
gst_amc_codec_jni_stop (GstAmcCodec * codec, GError ** err)
{
  JNIEnv *env;

  g_return_val_if_fail (codec != NULL, FALSE);

  env = gst_amc_jni_get_env ();

  if (codec->input_buffers)
    gst_amc_jni_free_buffer_array (env, codec->input_buffers,
        codec->n_input_buffers);
  codec->input_buffers = NULL;
  codec->n_input_buffers = 0;

  if (codec->output_buffers)
    gst_amc_jni_free_buffer_array (env, codec->output_buffers,
        codec->n_output_buffers);
  codec->output_buffers = NULL;
  codec->n_output_buffers = 0;

  return gst_amc_jni_call_void_method (env, err, codec->object,
      media_codec.stop);
}

static gboolean
gst_amc_codec_jni_flush (GstAmcCodec * codec, GError ** err)
{
  JNIEnv *env;

  g_return_val_if_fail (codec != NULL, FALSE);

  env = gst_amc_jni_get_env ();
  return gst_amc_jni_call_void_method (env, err, codec->object,
      media_codec.flush);
}

static gboolean
gst_amc_codec_jni_set_parameter (GstAmcCodec * codec, JNIEnv * env,
    GError ** err, const gchar * key, int value)
{
  gboolean ret = FALSE;
  jobject bundle = NULL;
  jstring jkey = NULL;

  if (media_codec.setParameters == NULL)
    goto done;                  // Not available means we're on Android < 19

  bundle = gst_amc_jni_new_object (env, err, FALSE, bundle_class.klass,
      bundle_class.constructor);
  if (!bundle)
    goto done;

  jkey = (*env)->NewStringUTF (env, key);
  if (!gst_amc_jni_call_void_method (env, err,
          bundle, bundle_class.putInt, jkey, value))
    goto done;

  if (!gst_amc_jni_call_void_method (env, err, codec->object,
          media_codec.setParameters, bundle))
    goto done;

  ret = TRUE;
done:
  if (jkey)
    (*env)->DeleteLocalRef (env, jkey);
  if (bundle)
    (*env)->DeleteLocalRef (env, bundle);
  return ret;
}

static gboolean
gst_amc_codec_jni_request_key_frame (GstAmcCodec * codec, GError ** err)
{
  JNIEnv *env;

  g_return_val_if_fail (codec != NULL, FALSE);

  env = gst_amc_jni_get_env ();
  return gst_amc_codec_jni_set_parameter (codec, env, err,
      PARAMETER_KEY_REQUEST_SYNC_FRAME, 0);
}

static gboolean
gst_amc_codec_jni_have_dynamic_bitrate ()
{
  /* Dynamic bitrate scaling is supported on Android >= 19,
   * where the setParameters() call is available */
  return (media_codec.setParameters != NULL);
}

static gboolean
gst_amc_codec_jni_set_dynamic_bitrate (GstAmcCodec * codec, GError ** err,
    gint bitrate)
{
  JNIEnv *env;

  g_return_val_if_fail (codec != NULL, FALSE);

  env = gst_amc_jni_get_env ();
  return gst_amc_codec_jni_set_parameter (codec, env, err,
      PARAMETER_KEY_VIDEO_BITRATE, bitrate);
}

static gboolean
gst_amc_codec_jni_release (GstAmcCodec * codec, GError ** err)
{
  JNIEnv *env;

  g_return_val_if_fail (codec != NULL, FALSE);

  env = gst_amc_jni_get_env ();

  if (codec->input_buffers)
    gst_amc_jni_free_buffer_array (env, codec->input_buffers,
        codec->n_input_buffers);
  codec->input_buffers = NULL;
  codec->n_input_buffers = 0;

  if (codec->output_buffers)
    gst_amc_jni_free_buffer_array (env, codec->output_buffers,
        codec->n_output_buffers);
  codec->output_buffers = NULL;
  codec->n_output_buffers = 0;

  return gst_amc_jni_call_void_method (env, err, codec->object,
      media_codec.release);
}

static GstAmcBuffer *
gst_amc_codec_jni_get_output_buffer (GstAmcCodec * codec, gint index,
    GError ** err)
{
  JNIEnv *env;
  jobject buffer = NULL;
  RealBuffer *ret = NULL;

  g_return_val_if_fail (codec != NULL, NULL);
  g_return_val_if_fail (index >= 0, NULL);

  env = gst_amc_jni_get_env ();

  if (!media_codec.get_output_buffer) {
    g_return_val_if_fail (index < codec->n_output_buffers && index >= 0, NULL);
    if (codec->output_buffers[index].object)
      return gst_amc_buffer_copy (&codec->output_buffers[index]);
    else
      return NULL;
  }

  if (!gst_amc_jni_call_object_method (env, err, codec->object,
          media_codec.get_output_buffer, &buffer, index))
    goto done;

  if (buffer != NULL) {
    ret = g_new0 (RealBuffer, 1);
    ret->object = gst_amc_jni_object_make_global (env, buffer);
    if (!ret->object) {
      gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
          GST_LIBRARY_ERROR_FAILED, "Failed to create global buffer reference");
      goto error;
    }

    ret->data = (*env)->GetDirectBufferAddress (env, ret->object);
    if (!ret->data) {
      gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
          GST_LIBRARY_ERROR_FAILED, "Failed to get buffer address");
      goto error;
    }
    ret->size = (*env)->GetDirectBufferCapacity (env, ret->object);
  }

done:

  return (GstAmcBuffer *) ret;

error:
  if (ret->object)
    gst_amc_jni_object_unref (env, ret->object);
  g_free (ret);

  return NULL;
}

static GstAmcBuffer *
gst_amc_codec_jni_get_input_buffer (GstAmcCodec * codec, gint index,
    GError ** err)
{
  JNIEnv *env;
  jobject buffer = NULL;
  RealBuffer *ret = NULL;

  g_return_val_if_fail (codec != NULL, NULL);
  g_return_val_if_fail (index >= 0, NULL);

  env = gst_amc_jni_get_env ();

  if (!media_codec.get_input_buffer) {
    g_return_val_if_fail (index < codec->n_input_buffers && index >= 0, NULL);
    if (codec->input_buffers[index].object)
      return gst_amc_buffer_copy (&codec->input_buffers[index]);
    else
      return NULL;
  }

  if (!gst_amc_jni_call_object_method (env, err, codec->object,
          media_codec.get_input_buffer, &buffer, index))
    goto done;

  if (buffer != NULL) {
    ret = g_new0 (RealBuffer, 1);
    ret->object = gst_amc_jni_object_make_global (env, buffer);
    if (!ret->object) {
      gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
          GST_LIBRARY_ERROR_FAILED, "Failed to create global buffer reference");
      goto error;
    }

    ret->data = (*env)->GetDirectBufferAddress (env, ret->object);
    if (!ret->data) {
      gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
          GST_LIBRARY_ERROR_FAILED, "Failed to get buffer address");
      goto error;
    }
    ret->size = (*env)->GetDirectBufferCapacity (env, ret->object);
  }

done:

  return (GstAmcBuffer *) ret;

error:
  if (ret->object)
    gst_amc_jni_object_unref (env, ret->object);
  g_free (ret);

  return NULL;
}

static gint
gst_amc_codec_jni_dequeue_input_buffer (GstAmcCodec * codec, gint64 timeoutUs,
    GError ** err)
{
  JNIEnv *env;
  gint ret = G_MININT;

  g_return_val_if_fail (codec != NULL, G_MININT);

  env = gst_amc_jni_get_env ();
  if (!gst_amc_jni_call_int_method (env, err, codec->object,
          media_codec.dequeue_input_buffer, &ret, timeoutUs))
    return G_MININT;

  return ret;
}

static gboolean
gst_amc_codec_jni_fill_buffer_info (JNIEnv * env,
    jobject buffer_info, GstAmcBufferInfo * info, GError ** err)
{
  g_return_val_if_fail (buffer_info != NULL, FALSE);

  if (!gst_amc_jni_get_int_field (env, err, buffer_info,
          media_codec_buffer_info.flags, &info->flags))
    return FALSE;

  if (!gst_amc_jni_get_int_field (env, err, buffer_info,
          media_codec_buffer_info.offset, &info->offset))
    return FALSE;

  if (!gst_amc_jni_get_long_field (env, err, buffer_info,
          media_codec_buffer_info.presentation_time_us,
          &info->presentation_time_us))
    return FALSE;

  if (!gst_amc_jni_get_int_field (env, err, buffer_info,
          media_codec_buffer_info.size, &info->size))
    return FALSE;

  return TRUE;
}

static gint
gst_amc_codec_jni_dequeue_output_buffer (GstAmcCodec * codec,
    GstAmcBufferInfo * info, gint64 timeoutUs, GError ** err)
{
  JNIEnv *env;
  gint ret = G_MININT;
  jobject info_o = NULL;

  g_return_val_if_fail (codec != NULL, G_MININT);

  env = gst_amc_jni_get_env ();

  info_o =
      gst_amc_jni_new_object (env, err, FALSE, media_codec_buffer_info.klass,
      media_codec_buffer_info.constructor);
  if (!info_o)
    goto done;

  if (!gst_amc_jni_call_int_method (env, err, codec->object,
          media_codec.dequeue_output_buffer, &ret, info_o, timeoutUs)) {
    ret = G_MININT;
    goto done;
  }

  if (ret == INFO_OUTPUT_BUFFERS_CHANGED || ret == INFO_OUTPUT_FORMAT_CHANGED
      || (ret >= 0 && !codec->output_buffers
          && !media_codec.get_output_buffer)) {
    if (!media_codec.get_output_buffer) {
      if (codec->output_buffers)
        gst_amc_jni_free_buffer_array (env, codec->output_buffers,
            codec->n_output_buffers);
      codec->output_buffers =
          gst_amc_codec_jni_get_output_buffers (codec, &codec->n_output_buffers,
          err);
      if (!codec->output_buffers) {
        ret = G_MININT;
        goto done;
      }
    }
    if (ret == INFO_OUTPUT_BUFFERS_CHANGED) {
      gst_amc_jni_object_local_unref (env, info_o);
      return gst_amc_codec_dequeue_output_buffer (codec, info, timeoutUs, err);
    }
  } else if (ret < 0) {
    goto done;
  }

  if (ret >= 0 && !gst_amc_codec_jni_fill_buffer_info (env, info_o, info, err)) {
    ret = G_MININT;
    goto done;
  }

done:
  if (info_o)
    gst_amc_jni_object_local_unref (env, info_o);
  info_o = NULL;

  return ret;
}

static gboolean
gst_amc_codec_jni_queue_input_buffer (GstAmcCodec * codec, gint index,
    const GstAmcBufferInfo * info, GError ** err)
{
  JNIEnv *env;

  g_return_val_if_fail (codec != NULL, FALSE);
  g_return_val_if_fail (info != NULL, FALSE);

  env = gst_amc_jni_get_env ();
  return gst_amc_jni_call_void_method (env, err, codec->object,
      media_codec.queue_input_buffer, index, info->offset, info->size,
      info->presentation_time_us, info->flags);
}

static gboolean
gst_amc_codec_jni_release_output_buffer (GstAmcCodec * codec, gint index,
    gboolean render, GError ** err)
{
  JNIEnv *env;

  g_return_val_if_fail (codec != NULL, FALSE);

  env = gst_amc_jni_get_env ();
  return gst_amc_jni_call_void_method (env, err, codec->object,
      media_codec.release_output_buffer, index, render);
}

static GstAmcSurfaceTexture *
gst_amc_codec_jni_new_surface_texture (GError ** err)
{
  return (GstAmcSurfaceTexture *) gst_amc_surface_texture_jni_new (err);
}

GstAmcCodecVTable gst_amc_codec_jni_vtable = {
  .buffer_free = gst_amc_buffer_jni_free,
  .buffer_set_position_and_limit = gst_amc_buffer_jni_set_position_and_limit,

  .create = gst_amc_codec_jni_new,
  .free = gst_amc_codec_jni_free,

  .configure = gst_amc_codec_jni_configure,
  .get_output_format = gst_amc_codec_jni_get_output_format,

  .start = gst_amc_codec_jni_start,
  .stop = gst_amc_codec_jni_stop,
  .flush = gst_amc_codec_jni_flush,
  .request_key_frame = gst_amc_codec_jni_request_key_frame,

  .have_dynamic_bitrate = gst_amc_codec_jni_have_dynamic_bitrate,
  .set_dynamic_bitrate = gst_amc_codec_jni_set_dynamic_bitrate,

  .release = gst_amc_codec_jni_release,

  .get_output_buffer = gst_amc_codec_jni_get_output_buffer,
  .get_input_buffer = gst_amc_codec_jni_get_input_buffer,

  .dequeue_input_buffer = gst_amc_codec_jni_dequeue_input_buffer,
  .dequeue_output_buffer = gst_amc_codec_jni_dequeue_output_buffer,

  .queue_input_buffer = gst_amc_codec_jni_queue_input_buffer,
  .release_output_buffer = gst_amc_codec_jni_release_output_buffer,

  .new_surface_texture = gst_amc_codec_jni_new_surface_texture,
};
