/*
 * Copyright (C) 2012, Collabora Ltd.
 * Copyright (C) 2012, Cisco Systems, Inc.
 *   Author: Youness Alaoui <youness.alaoui@collabora.co.uk>
 *
 * Copyright (C) 2015, Collabora Ltd.
 *   Author: Justin Kim <justin.kim@collabora.com>
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

#include "gstjniutils.h"

#include "gst-android-graphics-imageformat.h"


static struct
{
  jclass klass;
  jmethodID get_bits_per_pixel;
  jint JPEG;
  jint NV16;
  jint NV21;
  jint RGB_565;
  jint UNKNOWN;
  jint YUY2;
  jint YV12;
} android_graphics_imageformat = {
  0
};

gint ImageFormat_JPEG;
gint ImageFormat_NV16;
gint ImageFormat_NV21;
gint ImageFormat_RGB_565;
gint ImageFormat_UNKNOWN;
gint ImageFormat_YUY2;
gint ImageFormat_YV12;

static gboolean
_init_classes (void)
{
  JNIEnv *env;

  jfieldID fieldID;
  jclass klass;

  GError *err = NULL;

  env = gst_amc_jni_get_env ();

  /* android.graphics.ImageFormat */
  klass = android_graphics_imageformat.klass =
      gst_amc_jni_get_class (env, &err, "android/graphics/ImageFormat");

  if (err)
    goto failed;

  android_graphics_imageformat.get_bits_per_pixel =
      gst_amc_jni_get_static_method_id (env, &err, klass,
      "getBitsPerPixel", "(I)I");

  if (err)
    goto failed;

  fieldID = gst_amc_jni_get_static_field_id (env, &err, klass, "JPEG", "I");
  if (err)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID,
          &android_graphics_imageformat.JPEG))
    goto failed;
  ImageFormat_JPEG = android_graphics_imageformat.JPEG;

  fieldID = gst_amc_jni_get_static_field_id (env, &err, klass, "NV16", "I");
  if (err)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID,
          &android_graphics_imageformat.NV16))
    goto failed;
  ImageFormat_NV16 = android_graphics_imageformat.NV16;

  fieldID = gst_amc_jni_get_static_field_id (env, &err, klass, "NV21", "I");
  if (err)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID,
          &android_graphics_imageformat.NV21))
    goto failed;
  ImageFormat_NV21 = android_graphics_imageformat.NV21;

  fieldID = gst_amc_jni_get_static_field_id (env, &err, klass, "RGB_565", "I");
  if (err)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID,
          &android_graphics_imageformat.RGB_565))
    goto failed;
  ImageFormat_RGB_565 = android_graphics_imageformat.RGB_565;

  fieldID = gst_amc_jni_get_static_field_id (env, &err, klass, "UNKNOWN", "I");
  if (err)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID,
          &android_graphics_imageformat.UNKNOWN))
    goto failed;
  ImageFormat_UNKNOWN = android_graphics_imageformat.UNKNOWN;

  fieldID = gst_amc_jni_get_static_field_id (env, &err, klass, "YUY2", "I");
  if (err)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID,
          &android_graphics_imageformat.YUY2))
    goto failed;
  ImageFormat_YUY2 = android_graphics_imageformat.YUY2;

  fieldID = gst_amc_jni_get_static_field_id (env, &err, klass, "YV12", "I");
  if (err)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID,
          &android_graphics_imageformat.YV12))
    goto failed;
  ImageFormat_YV12 = android_graphics_imageformat.YV12;

  return TRUE;

failed:
  if (err) {
    GST_ERROR ("Failed to get android.graphics.ImageFormat class: %s",
        err->message);
    g_clear_error (&err);
  }

  return FALSE;
}

gboolean
gst_android_graphics_imageformat_init (void)
{
  if (!_init_classes ()) {
    gst_android_graphics_imageformat_deinit ();
    return FALSE;
  }

  return TRUE;
}

void
gst_android_graphics_imageformat_deinit (void)
{
  JNIEnv *env = gst_amc_jni_get_env ();

  if (android_graphics_imageformat.klass)
    (*env)->DeleteGlobalRef (env, android_graphics_imageformat.klass);
  android_graphics_imageformat.klass = NULL;
}

/* android.graphics.ImageFormat */
gint
gst_ag_imageformat_get_bits_per_pixel (gint format)
{
  JNIEnv *env;
  GError *err = NULL;

  jclass klass = android_graphics_imageformat.klass;

  jint bpp = 0;

  env = gst_amc_jni_get_env ();

  if (!gst_amc_jni_call_static_int_method (env, &err,
          klass, android_graphics_imageformat.get_bits_per_pixel, &bpp,
          format)) {
    GST_ERROR ("Failed to get android.graphics.ImageFormat class: %s",
        err->message);
    g_clear_error (&err);
  }

  return bpp;
}
