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
#include "../gstamc-codeclist.h"

#include "gstamc-jni.h"

struct _GstAmcCodecInfoHandle
{
  jobject object;
};

struct _GstAmcCodecCapabilitiesHandle
{
  jobject object;
};

static struct
{
  jclass klass;
  jmethodID get_codec_count;
  jmethodID get_codec_info_at;
} media_codeclist;

static struct
{
  jclass klass;
  jmethodID get_capabilities_for_type;
  jmethodID get_name;
  jmethodID get_supported_types;
  jmethodID is_encoder;
} media_codecinfo;

static struct
{
  jclass klass;
  jfieldID color_formats;
  jfieldID profile_levels;
} media_codeccapabilities;

static struct
{
  jclass klass;
  jfieldID level;
  jfieldID profile;
} media_codecprofilelevel;

gboolean
gst_amc_codeclist_jni_static_init (void)
{
  JNIEnv *env;
  GError *err = NULL;

  env = gst_amc_jni_get_env ();

  media_codeclist.klass =
      gst_amc_jni_get_class (env, &err, "android/media/MediaCodecList");
  if (!media_codeclist.klass) {
    GST_ERROR ("Failed to get android.media.MediaCodecList class: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  media_codeclist.get_codec_count =
      gst_amc_jni_get_static_method_id (env, &err, media_codeclist.klass,
      "getCodecCount", "()I");
  if (!media_codeclist.get_codec_count) {
    GST_ERROR ("Failed to get android.media.MediaCodecList getCodecCount(): %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  media_codeclist.get_codec_info_at =
      gst_amc_jni_get_static_method_id (env, &err, media_codeclist.klass,
      "getCodecInfoAt", "(I)Landroid/media/MediaCodecInfo;");
  if (!media_codeclist.get_codec_count) {
    GST_ERROR
        ("Failed to get android.media.MediaCodecList getCodecInfoAt(): %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  media_codecinfo.klass =
      gst_amc_jni_get_class (env, &err, "android/media/MediaCodecInfo");
  if (!media_codecinfo.klass) {
    GST_ERROR ("Failed to get android.media.MediaCodecInfo class: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  media_codecinfo.get_capabilities_for_type =
      gst_amc_jni_get_method_id (env, &err, media_codecinfo.klass,
      "getCapabilitiesForType",
      "(Ljava/lang/String;)Landroid/media/MediaCodecInfo$CodecCapabilities;");
  if (!media_codecinfo.get_capabilities_for_type) {
    GST_ERROR
        ("Failed to get android.media.MediaCodecInfo getCapabilitiesForType(): %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  media_codecinfo.get_name =
      gst_amc_jni_get_method_id (env, &err, media_codecinfo.klass, "getName",
      "()Ljava/lang/String;");
  if (!media_codecinfo.get_name) {
    GST_ERROR ("Failed to get android.media.MediaCodecInfo getName(): %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  media_codecinfo.get_supported_types =
      gst_amc_jni_get_method_id (env, &err, media_codecinfo.klass,
      "getSupportedTypes", "()[Ljava/lang/String;");
  if (!media_codecinfo.get_supported_types) {
    GST_ERROR
        ("Failed to get android.media.MediaCodecInfo getSupportedTypes(): %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  media_codecinfo.is_encoder =
      gst_amc_jni_get_method_id (env, &err, media_codecinfo.klass, "isEncoder",
      "()Z");
  if (!media_codecinfo.is_encoder) {
    GST_ERROR ("Failed to get android.media.MediaCodecInfo isEncoder(): %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  media_codeccapabilities.klass =
      gst_amc_jni_get_class (env, &err,
      "android/media/MediaCodecInfo$CodecCapabilities");
  if (!media_codeccapabilities.klass) {
    GST_ERROR
        ("Failed to get android.media.MediaCodecInfo.CodecCapabilities class: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  media_codeccapabilities.color_formats =
      gst_amc_jni_get_field_id (env, &err, media_codeccapabilities.klass,
      "colorFormats", "[I");
  if (!media_codeccapabilities.color_formats) {
    GST_ERROR
        ("Failed to get android.media.MediaCodecInfo.CodecCapabilities colorFormats: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  media_codeccapabilities.profile_levels =
      gst_amc_jni_get_field_id (env, &err, media_codeccapabilities.klass,
      "profileLevels", "[Landroid/media/MediaCodecInfo$CodecProfileLevel;");
  if (!media_codeccapabilities.profile_levels) {
    GST_ERROR
        ("Failed to get android.media.MediaCodecInfo.CodecCapabilities profileLevels: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  media_codecprofilelevel.klass =
      gst_amc_jni_get_class (env, &err,
      "android/media/MediaCodecInfo$CodecProfileLevel");
  if (!media_codecprofilelevel.klass) {
    GST_ERROR
        ("Failed to get android.media.MediaCodecInfo.CodecProfileLevel class: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  media_codecprofilelevel.level =
      gst_amc_jni_get_field_id (env, &err, media_codecprofilelevel.klass,
      "level", "I");
  if (!media_codecprofilelevel.level) {
    GST_ERROR
        ("Failed to get android.media.MediaCodecInfo.CodecProfileLevel level: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  media_codecprofilelevel.profile =
      gst_amc_jni_get_field_id (env, &err, media_codecprofilelevel.klass,
      "profile", "I");
  if (!media_codecprofilelevel.profile) {
    GST_ERROR
        ("Failed to get android.media.MediaCodecInfo.CodecProfileLevel profile: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_amc_codeclist_get_count (gint * count, GError ** err)
{
  JNIEnv *env;

  env = gst_amc_jni_get_env ();

  if (!gst_amc_jni_call_static_int_method (env, err, media_codeclist.klass,
          media_codeclist.get_codec_count, count))
    return FALSE;

  return TRUE;
}

GstAmcCodecInfoHandle *
gst_amc_codeclist_get_codec_info_at (gint index, GError ** err)
{
  GstAmcCodecInfoHandle *ret;
  jobject object;
  JNIEnv *env;

  env = gst_amc_jni_get_env ();

  if (!gst_amc_jni_call_static_object_method (env, err, media_codeclist.klass,
          media_codeclist.get_codec_info_at, &object, index))
    return NULL;

  ret = g_new0 (GstAmcCodecInfoHandle, 1);
  ret->object = object;
  return ret;
}

void
gst_amc_codec_info_handle_free (GstAmcCodecInfoHandle * handle)
{
  JNIEnv *env;

  g_return_if_fail (handle != NULL);

  env = gst_amc_jni_get_env ();

  if (handle->object)
    gst_amc_jni_object_local_unref (env, handle->object);
  g_free (handle);
}

gchar *
gst_amc_codec_info_handle_get_name (GstAmcCodecInfoHandle * handle,
    GError ** err)
{
  JNIEnv *env;
  jstring v_str = NULL;

  g_return_val_if_fail (handle != NULL, NULL);

  env = gst_amc_jni_get_env ();

  if (!gst_amc_jni_call_object_method (env, err, handle->object,
          media_codecinfo.get_name, &v_str))
    return NULL;

  return gst_amc_jni_string_to_gchar (env, v_str, TRUE);
}

gboolean
gst_amc_codec_info_handle_is_encoder (GstAmcCodecInfoHandle * handle,
    gboolean * is_encoder, GError ** err)
{
  JNIEnv *env;

  g_return_val_if_fail (handle != NULL, FALSE);
  g_return_val_if_fail (is_encoder != NULL, FALSE);

  env = gst_amc_jni_get_env ();

  if (!gst_amc_jni_call_boolean_method (env, err, handle->object,
          media_codecinfo.is_encoder, is_encoder))
    return FALSE;

  return TRUE;
}

gchar **
gst_amc_codec_info_handle_get_supported_types (GstAmcCodecInfoHandle * handle,
    gsize * length, GError ** err)
{
  JNIEnv *env;
  jarray array = NULL;
  jsize len;
  jsize i;
  gchar **strv = NULL;

  g_return_val_if_fail (handle != NULL, NULL);

  env = gst_amc_jni_get_env ();

  if (!gst_amc_jni_call_object_method (env, err, handle->object,
          media_codecinfo.get_supported_types, &array))
    goto done;

  len = (*env)->GetArrayLength (env, array);
  if ((*env)->ExceptionCheck (env)) {
    gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
        GST_LIBRARY_ERROR_FAILED, "Failed to get array length");
    goto done;
  }

  strv = g_new0 (gchar *, len + 1);
  *length = len;

  for (i = 0; i < len; i++) {
    jstring string;

    string = (*env)->GetObjectArrayElement (env, array, i);
    if ((*env)->ExceptionCheck (env)) {
      gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
          GST_LIBRARY_ERROR_FAILED, "Failed to get array element");
      g_strfreev (strv);
      strv = NULL;
      goto done;
    }

    strv[i] = gst_amc_jni_string_to_gchar (env, string, TRUE);
    if (!strv[i]) {
      gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
          GST_LIBRARY_ERROR_FAILED, "Failed create string");
      g_strfreev (strv);
      strv = NULL;
      goto done;
    }
  }

done:
  if (array)
    (*env)->DeleteLocalRef (env, array);

  return strv;
}

GstAmcCodecCapabilitiesHandle *
gst_amc_codec_info_handle_get_capabilities_for_type (GstAmcCodecInfoHandle *
    handle, const gchar * type, GError ** err)
{
  GstAmcCodecCapabilitiesHandle *ret = NULL;
  jstring type_str;
  jobject object;
  JNIEnv *env;

  env = gst_amc_jni_get_env ();

  type_str = gst_amc_jni_string_from_gchar (env, err, FALSE, type);
  if (!type_str)
    goto done;

  if (!gst_amc_jni_call_object_method (env, err, handle->object,
          media_codecinfo.get_capabilities_for_type, &object, type_str))
    goto done;

  ret = g_new0 (GstAmcCodecCapabilitiesHandle, 1);
  ret->object = object;

done:
  if (type_str)
    gst_amc_jni_object_local_unref (env, type_str);

  return ret;
}

void
gst_amc_codec_capabilities_handle_free (GstAmcCodecCapabilitiesHandle * handle)
{
  JNIEnv *env;

  g_return_if_fail (handle != NULL);

  env = gst_amc_jni_get_env ();

  if (handle->object)
    gst_amc_jni_object_local_unref (env, handle->object);
  g_free (handle);
}

gint *gst_amc_codec_capabilities_handle_get_color_formats
    (GstAmcCodecCapabilitiesHandle * handle, gsize * length, GError ** err)
{
  JNIEnv *env;
  jarray array = NULL;
  jsize len;
  jint *elems = NULL;
  gint *ret = NULL;

  g_return_val_if_fail (handle != NULL, NULL);

  env = gst_amc_jni_get_env ();

  if (!gst_amc_jni_get_object_field (env, err, handle->object,
          media_codeccapabilities.color_formats, &array))
    goto done;

  len = (*env)->GetArrayLength (env, array);
  if ((*env)->ExceptionCheck (env)) {
    gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
        GST_LIBRARY_ERROR_FAILED, "Failed to get array length");
    goto done;
  }

  elems = (*env)->GetIntArrayElements (env, array, NULL);
  if ((*env)->ExceptionCheck (env)) {
    gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
        GST_LIBRARY_ERROR_FAILED, "Failed to get array elements");
    goto done;
  }

  ret = g_memdup2 (elems, sizeof (jint) * len);
  *length = len;

done:
  if (elems)
    (*env)->ReleaseIntArrayElements (env, array, elems, JNI_ABORT);
  if (array)
    (*env)->DeleteLocalRef (env, array);

  return ret;
}

GstAmcCodecProfileLevel *gst_amc_codec_capabilities_handle_get_profile_levels
    (GstAmcCodecCapabilitiesHandle * handle, gsize * length, GError ** err)
{
  GstAmcCodecProfileLevel *ret = NULL;
  JNIEnv *env;
  jobject array = NULL;
  jsize len;
  jsize i;

  env = gst_amc_jni_get_env ();

  if (!gst_amc_jni_get_object_field (env, err, handle->object,
          media_codeccapabilities.profile_levels, &array))
    goto done;

  len = (*env)->GetArrayLength (env, array);
  if ((*env)->ExceptionCheck (env)) {
    gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
        GST_LIBRARY_ERROR_FAILED, "Failed to get array length");
    goto done;
  }

  ret = g_new0 (GstAmcCodecProfileLevel, len);
  *length = len;

  for (i = 0; i < len; i++) {
    jobject object = NULL;

    object = (*env)->GetObjectArrayElement (env, array, i);
    if ((*env)->ExceptionCheck (env)) {
      gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
          GST_LIBRARY_ERROR_FAILED, "Failed to get array element");
      g_free (ret);
      ret = NULL;
      goto done;
    }

    if (!gst_amc_jni_get_int_field (env, err, object,
            media_codecprofilelevel.level, &ret[i].level)) {
      gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
          GST_LIBRARY_ERROR_FAILED, "Failed to get level");
      (*env)->DeleteLocalRef (env, object);
      g_free (ret);
      ret = NULL;
      goto done;
    }

    if (!gst_amc_jni_get_int_field (env, err, object,
            media_codecprofilelevel.profile, &ret[i].profile)) {
      gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
          GST_LIBRARY_ERROR_FAILED, "Failed to get profile");
      (*env)->DeleteLocalRef (env, object);
      g_free (ret);
      ret = NULL;
      goto done;
    }

    (*env)->DeleteLocalRef (env, object);
  }

done:
  if (array)
    (*env)->DeleteLocalRef (env, array);

  return ret;
}
