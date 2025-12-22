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
#include "../gstamc-format.h"
#include "gstamc-jni.h"
#include "gstamc-internal-jni.h"

static struct
{
  jclass klass;
  jmethodID create_audio_format;
  jmethodID create_video_format;
  jmethodID to_string;
  jmethodID get_float;
  jmethodID set_float;
  jmethodID get_integer;
  jmethodID set_integer;
  jmethodID get_string;
  jmethodID set_string;
  jmethodID get_byte_buffer;
  jmethodID set_byte_buffer;
} media_format;

gboolean
gst_amc_format_jni_static_init (void)
{
  gboolean ret = TRUE;
  JNIEnv *env;
  jclass tmp;

  env = gst_amc_jni_get_env ();

  tmp = (*env)->FindClass (env, "android/media/MediaFormat");
  if (!tmp) {
    ret = FALSE;
    GST_ERROR ("Failed to get format class");
    if ((*env)->ExceptionCheck (env)) {
      (*env)->ExceptionDescribe (env);
      (*env)->ExceptionClear (env);
    }
    goto done;
  }
  media_format.klass = (*env)->NewGlobalRef (env, tmp);
  if (!media_format.klass) {
    ret = FALSE;
    GST_ERROR ("Failed to get format class global reference");
    if ((*env)->ExceptionCheck (env)) {
      (*env)->ExceptionDescribe (env);
      (*env)->ExceptionClear (env);
    }
    goto done;
  }
  (*env)->DeleteLocalRef (env, tmp);
  tmp = NULL;

  media_format.create_audio_format =
      (*env)->GetStaticMethodID (env, media_format.klass, "createAudioFormat",
      "(Ljava/lang/String;II)Landroid/media/MediaFormat;");
  media_format.create_video_format =
      (*env)->GetStaticMethodID (env, media_format.klass, "createVideoFormat",
      "(Ljava/lang/String;II)Landroid/media/MediaFormat;");
  media_format.to_string =
      (*env)->GetMethodID (env, media_format.klass, "toString",
      "()Ljava/lang/String;");
  media_format.get_float =
      (*env)->GetMethodID (env, media_format.klass, "getFloat",
      "(Ljava/lang/String;)F");
  media_format.set_float =
      (*env)->GetMethodID (env, media_format.klass, "setFloat",
      "(Ljava/lang/String;F)V");
  media_format.get_integer =
      (*env)->GetMethodID (env, media_format.klass, "getInteger",
      "(Ljava/lang/String;)I");
  media_format.set_integer =
      (*env)->GetMethodID (env, media_format.klass, "setInteger",
      "(Ljava/lang/String;I)V");
  media_format.get_string =
      (*env)->GetMethodID (env, media_format.klass, "getString",
      "(Ljava/lang/String;)Ljava/lang/String;");
  media_format.set_string =
      (*env)->GetMethodID (env, media_format.klass, "setString",
      "(Ljava/lang/String;Ljava/lang/String;)V");
  media_format.get_byte_buffer =
      (*env)->GetMethodID (env, media_format.klass, "getByteBuffer",
      "(Ljava/lang/String;)Ljava/nio/ByteBuffer;");
  media_format.set_byte_buffer =
      (*env)->GetMethodID (env, media_format.klass, "setByteBuffer",
      "(Ljava/lang/String;Ljava/nio/ByteBuffer;)V");
  if (!media_format.create_audio_format || !media_format.create_video_format
      || !media_format.get_float || !media_format.set_float
      || !media_format.get_integer || !media_format.set_integer
      || !media_format.get_string || !media_format.set_string
      || !media_format.get_byte_buffer || !media_format.set_byte_buffer) {
    ret = FALSE;
    GST_ERROR ("Failed to get format methods");
    if ((*env)->ExceptionCheck (env)) {
      (*env)->ExceptionDescribe (env);
      (*env)->ExceptionClear (env);
    }
    goto done;
  }

done:
  if (tmp)
    (*env)->DeleteLocalRef (env, tmp);
  tmp = NULL;

  return ret;
}

static GstAmcFormat *
gst_amc_format_jni_new_audio (const gchar * mime, gint sample_rate,
    gint channels, GError ** err)
{
  JNIEnv *env;
  GstAmcFormat *format = NULL;
  jstring mime_str;

  g_return_val_if_fail (mime != NULL, NULL);

  env = gst_amc_jni_get_env ();

  mime_str = gst_amc_jni_string_from_gchar (env, err, FALSE, mime);
  if (!mime_str)
    goto error;

  format = g_slice_new0 (GstAmcFormat);
  format->object =
      gst_amc_jni_new_object_from_static (env, err, TRUE, media_format.klass,
      media_format.create_audio_format, mime_str, sample_rate, channels);
  if (!format->object)
    goto error;

done:
  if (mime_str)
    gst_amc_jni_object_local_unref (env, mime_str);
  mime_str = NULL;

  return format;

error:
  if (format)
    g_slice_free (GstAmcFormat, format);
  format = NULL;
  goto done;
}

static GstAmcFormat *
gst_amc_format_jni_new_video (const gchar * mime, gint width, gint height,
    GError ** err)
{
  JNIEnv *env;
  GstAmcFormat *format = NULL;
  jstring mime_str;

  g_return_val_if_fail (mime != NULL, NULL);

  env = gst_amc_jni_get_env ();

  mime_str = gst_amc_jni_string_from_gchar (env, err, FALSE, mime);
  if (!mime_str)
    goto error;

  format = g_slice_new0 (GstAmcFormat);
  format->object =
      gst_amc_jni_new_object_from_static (env, err, TRUE, media_format.klass,
      media_format.create_video_format, mime_str, width, height);
  if (!format->object)
    goto error;

done:
  if (mime_str)
    gst_amc_jni_object_local_unref (env, mime_str);
  mime_str = NULL;

  return format;

error:
  if (format)
    g_slice_free (GstAmcFormat, format);
  format = NULL;
  goto done;
}

static void
gst_amc_format_jni_free (GstAmcFormat * format)
{
  JNIEnv *env;

  g_return_if_fail (format != NULL);

  env = gst_amc_jni_get_env ();
  gst_amc_jni_object_unref (env, format->object);
  g_slice_free (GstAmcFormat, format);
}

static gchar *
gst_amc_format_jni_to_string (GstAmcFormat * format, GError ** err)
{
  JNIEnv *env;
  jstring v_str = NULL;
  gchar *ret = NULL;

  g_return_val_if_fail (format != NULL, FALSE);

  env = gst_amc_jni_get_env ();

  if (!gst_amc_jni_call_object_method (env, err, format->object,
          media_format.to_string, &v_str))
    goto done;
  ret = gst_amc_jni_string_to_gchar (env, v_str, TRUE);

done:

  return ret;
}

static gboolean
gst_amc_format_jni_get_float (GstAmcFormat * format, const gchar * key,
    gfloat * value, GError ** err)
{
  JNIEnv *env;
  gboolean ret = FALSE;
  jstring key_str = NULL;

  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  *value = 0;
  env = gst_amc_jni_get_env ();

  key_str = gst_amc_jni_string_from_gchar (env, err, FALSE, key);
  if (!key_str)
    goto done;

  if (!gst_amc_jni_call_float_method (env, err, format->object,
          media_format.get_float, value, key_str))
    goto done;
  ret = TRUE;

done:
  if (key_str)
    gst_amc_jni_object_local_unref (env, key_str);

  return ret;
}

static gboolean
gst_amc_format_jni_set_float (GstAmcFormat * format, const gchar * key,
    gfloat value, GError ** err)
{
  JNIEnv *env;
  jstring key_str = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  env = gst_amc_jni_get_env ();

  key_str = gst_amc_jni_string_from_gchar (env, err, FALSE, key);
  if (!key_str)
    goto done;

  if (!gst_amc_jni_call_void_method (env, err, format->object,
          media_format.set_float, key_str, value))
    goto done;

  ret = TRUE;

done:
  if (key_str)
    gst_amc_jni_object_local_unref (env, key_str);

  return ret;
}

static gboolean
gst_amc_format_jni_get_int (GstAmcFormat * format, const gchar * key,
    gint * value, GError ** err)
{
  JNIEnv *env;
  gboolean ret = FALSE;
  jstring key_str = NULL;

  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  *value = 0;
  env = gst_amc_jni_get_env ();

  key_str = gst_amc_jni_string_from_gchar (env, err, FALSE, key);
  if (!key_str)
    goto done;

  if (!gst_amc_jni_call_int_method (env, err, format->object,
          media_format.get_integer, value, key_str))
    goto done;
  ret = TRUE;

done:
  if (key_str)
    gst_amc_jni_object_local_unref (env, key_str);

  return ret;

}

static gboolean
gst_amc_format_jni_set_int (GstAmcFormat * format, const gchar * key,
    gint value, GError ** err)
{
  JNIEnv *env;
  jstring key_str = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  env = gst_amc_jni_get_env ();

  key_str = gst_amc_jni_string_from_gchar (env, err, FALSE, key);
  if (!key_str)
    goto done;

  if (!gst_amc_jni_call_void_method (env, err, format->object,
          media_format.set_integer, key_str, value))
    goto done;

  ret = TRUE;

done:
  if (key_str)
    gst_amc_jni_object_local_unref (env, key_str);

  return ret;
}

static gboolean
gst_amc_format_jni_get_string (GstAmcFormat * format, const gchar * key,
    gchar ** value, GError ** err)
{
  JNIEnv *env;
  gboolean ret = FALSE;
  jstring key_str = NULL;
  jstring v_str = NULL;

  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  *value = 0;
  env = gst_amc_jni_get_env ();

  key_str = gst_amc_jni_string_from_gchar (env, err, FALSE, key);
  if (!key_str)
    goto done;

  if (!gst_amc_jni_call_object_method (env, err, format->object,
          media_format.get_string, &v_str, key_str))
    goto done;

  *value = gst_amc_jni_string_to_gchar (env, v_str, TRUE);

  ret = TRUE;

done:
  if (key_str)
    gst_amc_jni_object_local_unref (env, key_str);

  return ret;
}

static gboolean
gst_amc_format_jni_set_string (GstAmcFormat * format, const gchar * key,
    const gchar * value, GError ** err)
{
  JNIEnv *env;
  jstring key_str = NULL;
  jstring v_str = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  env = gst_amc_jni_get_env ();

  key_str = gst_amc_jni_string_from_gchar (env, err, FALSE, key);
  if (!key_str)
    goto done;

  v_str = gst_amc_jni_string_from_gchar (env, err, FALSE, value);
  if (!v_str)
    goto done;

  if (!gst_amc_jni_call_void_method (env, err, format->object,
          media_format.set_string, key_str, v_str))
    goto done;

  ret = TRUE;

done:
  if (key_str)
    gst_amc_jni_object_local_unref (env, key_str);
  if (v_str)
    gst_amc_jni_object_local_unref (env, v_str);

  return ret;
}

static gboolean
gst_amc_format_jni_get_buffer (GstAmcFormat * format, const gchar * key,
    guint8 ** data, gsize * size, GError ** err)
{
  JNIEnv *env;
  gboolean ret = FALSE;
  jstring key_str = NULL;
  jobject v = NULL;
  RealBuffer buf = { 0, };
  gint position = 0, limit = 0;

  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (size != NULL, FALSE);

  *data = NULL;
  *size = 0;
  env = gst_amc_jni_get_env ();

  key_str = gst_amc_jni_string_from_gchar (env, err, FALSE, key);
  if (!key_str)
    goto done;

  if (!gst_amc_jni_call_object_method (env, err, format->object,
          media_format.get_byte_buffer, &v, key_str))
    goto done;

  *data = (*env)->GetDirectBufferAddress (env, v);
  if (*data == NULL) {
    gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
        GST_LIBRARY_ERROR_FAILED, "Failed get buffer address");
    goto done;
  }
  *size = (*env)->GetDirectBufferCapacity (env, v);

  buf.object = v;
  buf.data = *data;
  buf.size = *size;
  gst_amc_buffer_get_position_and_limit (&buf, NULL, &position, &limit);
  *size = limit;

  *data = g_memdup2 (*data + position, limit);

  ret = TRUE;

done:
  if (key_str)
    gst_amc_jni_object_local_unref (env, key_str);
  if (v)
    gst_amc_jni_object_local_unref (env, v);

  return ret;
}

static gboolean
gst_amc_format_jni_set_buffer (GstAmcFormat * format, const gchar * key,
    guint8 * data, gsize size, GError ** err)
{
  JNIEnv *env;
  jstring key_str = NULL;
  jobject v = NULL;
  gboolean ret = FALSE;
  RealBuffer buf = { 0, };

  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  env = gst_amc_jni_get_env ();

  key_str = gst_amc_jni_string_from_gchar (env, err, FALSE, key);
  if (!key_str)
    goto done;

  /* FIXME: The memory must remain valid until the codec is stopped */
  v = (*env)->NewDirectByteBuffer (env, data, size);
  if (!v) {
    gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
        GST_LIBRARY_ERROR_FAILED, "Failed create Java byte buffer");
    goto done;
  }

  buf.object = v;
  buf.data = data;
  buf.size = size;

  gst_amc_buffer_set_position_and_limit ((GstAmcBuffer *) & buf, NULL, 0, size);

  if (!gst_amc_jni_call_void_method (env, err, format->object,
          media_format.set_byte_buffer, key_str, v))
    goto done;

  ret = TRUE;

done:
  if (key_str)
    gst_amc_jni_object_local_unref (env, key_str);
  if (v)
    gst_amc_jni_object_local_unref (env, v);

  return ret;
}

GstAmcFormatVTable gst_amc_format_jni_vtable = {
  .new_audio = gst_amc_format_jni_new_audio,
  .new_video = gst_amc_format_jni_new_video,
  .free = gst_amc_format_jni_free,

  .to_string = gst_amc_format_jni_to_string,

  .get_float = gst_amc_format_jni_get_float,
  .set_float = gst_amc_format_jni_set_float,
  .get_int = gst_amc_format_jni_get_int,
  .set_int = gst_amc_format_jni_set_int,
  .get_string = gst_amc_format_jni_get_string,
  .set_string = gst_amc_format_jni_set_string,
  .get_buffer = gst_amc_format_jni_get_buffer,
  .set_buffer = gst_amc_format_jni_set_buffer,
};
