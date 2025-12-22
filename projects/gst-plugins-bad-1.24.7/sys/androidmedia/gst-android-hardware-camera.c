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

#include "gst-android-hardware-camera.h"

GST_DEBUG_CATEGORY_STATIC (ahc_debug);
#define GST_CAT_DEFAULT ahc_debug

static struct
{
  jclass klass;
  jmethodID addCallbackBuffer;
  jmethodID autoFocus;
  jmethodID cancelAutoFocus;
  jmethodID getCameraInfo;
  jmethodID getNumberOfCameras;
  jmethodID getParameters;
  jmethodID lock;
  jmethodID open;
  jmethodID reconnect;
  jmethodID release;
  jmethodID setErrorCallback;
  jmethodID setParameters;
  jmethodID setPreviewCallbackWithBuffer;
  jmethodID setPreviewTexture;
  jmethodID startPreview;
  jmethodID startSmoothZoom;
  jmethodID stopPreview;
  jmethodID stopSmoothZoom;
  jmethodID unlock;
} android_hardware_camera = {
  0
};

static struct
{
  jclass klass;
  jmethodID constructor;
  jfieldID facing;
  jfieldID orientation;
  jint CAMERA_FACING_BACK;
  jint CAMERA_FACING_FRONT;
} android_hardware_camera_camerainfo = {
  0
};

gint CameraInfo_CAMERA_FACING_BACK;
gint CameraInfo_CAMERA_FACING_FRONT;

static struct
{
  jclass klass;
  jfieldID width;
  jfieldID height;
} android_hardware_camera_size = {
  0
};

static struct
{
  jclass klass;
  jmethodID flatten;
  jmethodID getAntibanding;
  jmethodID getColorEffect;
  jmethodID getExposureCompensation;
  jmethodID getExposureCompensationStep;
  jmethodID getFlashMode;
  jmethodID getFocalLength;
  jmethodID getFocusMode;
  jmethodID getHorizontalViewAngle;
  jmethodID getMaxExposureCompensation;
  jmethodID getMaxZoom;
  jmethodID getMinExposureCompensation;
  jmethodID getPreviewFormat;
  jmethodID getPreviewFpsRange;
  jmethodID getPreviewSize;
  jmethodID getSceneMode;
  jmethodID getSupportedAntibanding;
  jmethodID getSupportedColorEffects;
  jmethodID getSupportedFlashModes;
  jmethodID getSupportedFocusModes;
  jmethodID getSupportedPreviewFormats;
  jmethodID getSupportedPreviewFpsRange;
  jmethodID getSupportedPreviewSizes;
  jmethodID getSupportedSceneModes;
  jmethodID getSupportedWhiteBalance;
  jmethodID getVerticalViewAngle;
  jmethodID getVideoStabilization;
  jmethodID getWhiteBalance;
  jmethodID getZoom;
  jmethodID getZoomRatios;
  jmethodID isSmoothZoomSupported;
  jmethodID isVideoStabilizationSupported;
  jmethodID isZoomSupported;
  jmethodID setAntibanding;
  jmethodID setColorEffect;
  jmethodID setExposureCompensation;
  jmethodID setFlashMode;
  jmethodID setFocusMode;
  jmethodID setPreviewFormat;
  jmethodID setPreviewFpsRange;
  jmethodID setPreviewSize;
  jmethodID setSceneMode;
  jmethodID setVideoStabilization;
  jmethodID setWhiteBalance;
  jmethodID setZoom;
  jmethodID unflatten;
  jstring WHITE_BALANCE_AUTO;
  jstring WHITE_BALANCE_INCANDESCENT;
  jstring WHITE_BALANCE_FLUORESCENT;
  jstring WHITE_BALANCE_WARM_FLUORESCENT;
  jstring WHITE_BALANCE_DAYLIGHT;
  jstring WHITE_BALANCE_CLOUDY_DAYLIGHT;
  jstring WHITE_BALANCE_TWILIGHT;
  jstring WHITE_BALANCE_SHADE;
  jstring EFFECT_NONE;
  jstring EFFECT_MONO;
  jstring EFFECT_NEGATIVE;
  jstring EFFECT_SOLARIZE;
  jstring EFFECT_SEPIA;
  jstring EFFECT_POSTERIZE;
  jstring EFFECT_WHITEBOARD;
  jstring EFFECT_BLACKBOARD;
  jstring EFFECT_AQUA;
  jstring EFFECT_EMBOSS;
  jstring EFFECT_SKETCH;
  jstring EFFECT_NEON;
  jstring ANTIBANDING_AUTO;
  jstring ANTIBANDING_50HZ;
  jstring ANTIBANDING_60HZ;
  jstring ANTIBANDING_OFF;
  jstring FLASH_MODE_OFF;
  jstring FLASH_MODE_AUTO;
  jstring FLASH_MODE_ON;
  jstring FLASH_MODE_RED_EYE;
  jstring FLASH_MODE_TORCH;
  jstring SCENE_MODE_AUTO;
  jstring SCENE_MODE_ACTION;
  jstring SCENE_MODE_PORTRAIT;
  jstring SCENE_MODE_LANDSCAPE;
  jstring SCENE_MODE_NIGHT;
  jstring SCENE_MODE_NIGHT_PORTRAIT;
  jstring SCENE_MODE_THEATRE;
  jstring SCENE_MODE_BEACH;
  jstring SCENE_MODE_SNOW;
  jstring SCENE_MODE_SUNSET;
  jstring SCENE_MODE_STEADYPHOTO;
  jstring SCENE_MODE_FIREWORKS;
  jstring SCENE_MODE_SPORTS;
  jstring SCENE_MODE_PARTY;
  jstring SCENE_MODE_CANDLELIGHT;
  jstring SCENE_MODE_BARCODE;
  jstring SCENE_MODE_BACKLIGHT;
  jstring SCENE_MODE_FLOWERS;
  jstring SCENE_MODE_AR;
  jstring SCENE_MODE_HDR;
  jstring FOCUS_MODE_AUTO;
  jstring FOCUS_MODE_INFINITY;
  jstring FOCUS_MODE_MACRO;
  jstring FOCUS_MODE_FIXED;
  jstring FOCUS_MODE_EDOF;
  jstring FOCUS_MODE_CONTINUOUS_VIDEO;
  jstring FOCUS_MODE_CONTINUOUS_PICTURE;
} android_hardware_camera_parameters = {
  0
};

const gchar *Parameters_WHITE_BALANCE_AUTO;
const gchar *Parameters_WHITE_BALANCE_INCANDESCENT;
const gchar *Parameters_WHITE_BALANCE_FLUORESCENT;
const gchar *Parameters_WHITE_BALANCE_WARM_FLUORESCENT;
const gchar *Parameters_WHITE_BALANCE_DAYLIGHT;
const gchar *Parameters_WHITE_BALANCE_CLOUDY_DAYLIGHT;
const gchar *Parameters_WHITE_BALANCE_TWILIGHT;
const gchar *Parameters_WHITE_BALANCE_SHADE;
const gchar *Parameters_EFFECT_NONE;
const gchar *Parameters_EFFECT_MONO;
const gchar *Parameters_EFFECT_NEGATIVE;
const gchar *Parameters_EFFECT_SOLARIZE;
const gchar *Parameters_EFFECT_SEPIA;
const gchar *Parameters_EFFECT_POSTERIZE;
const gchar *Parameters_EFFECT_WHITEBOARD;
const gchar *Parameters_EFFECT_BLACKBOARD;
const gchar *Parameters_EFFECT_AQUA;
const gchar *Parameters_EFFECT_EMBOSS;
const gchar *Parameters_EFFECT_SKETCH;
const gchar *Parameters_EFFECT_NEON;
const gchar *Parameters_ANTIBANDING_AUTO;
const gchar *Parameters_ANTIBANDING_50HZ;
const gchar *Parameters_ANTIBANDING_60HZ;
const gchar *Parameters_ANTIBANDING_OFF;
const gchar *Parameters_FLASH_MODE_OFF;
const gchar *Parameters_FLASH_MODE_AUTO;
const gchar *Parameters_FLASH_MODE_ON;
const gchar *Parameters_FLASH_MODE_RED_EYE;
const gchar *Parameters_FLASH_MODE_TORCH;
const gchar *Parameters_SCENE_MODE_AUTO;
const gchar *Parameters_SCENE_MODE_ACTION;
const gchar *Parameters_SCENE_MODE_PORTRAIT;
const gchar *Parameters_SCENE_MODE_LANDSCAPE;
const gchar *Parameters_SCENE_MODE_NIGHT;
const gchar *Parameters_SCENE_MODE_NIGHT_PORTRAIT;
const gchar *Parameters_SCENE_MODE_THEATRE;
const gchar *Parameters_SCENE_MODE_BEACH;
const gchar *Parameters_SCENE_MODE_SNOW;
const gchar *Parameters_SCENE_MODE_SUNSET;
const gchar *Parameters_SCENE_MODE_STEADYPHOTO;
const gchar *Parameters_SCENE_MODE_FIREWORKS;
const gchar *Parameters_SCENE_MODE_SPORTS;
const gchar *Parameters_SCENE_MODE_PARTY;
const gchar *Parameters_SCENE_MODE_CANDLELIGHT;
const gchar *Parameters_SCENE_MODE_BARCODE;
const gchar *Parameters_SCENE_MODE_BACKLIGHT;
const gchar *Parameters_SCENE_MODE_FLOWERS;
const gchar *Parameters_SCENE_MODE_AR;
const gchar *Parameters_SCENE_MODE_HDR;
const gchar *Parameters_FOCUS_MODE_AUTO;
const gchar *Parameters_FOCUS_MODE_INFINITY;
const gchar *Parameters_FOCUS_MODE_MACRO;
const gchar *Parameters_FOCUS_MODE_FIXED;
const gchar *Parameters_FOCUS_MODE_EDOF;
const gchar *Parameters_FOCUS_MODE_CONTINUOUS_VIDEO;
const gchar *Parameters_FOCUS_MODE_CONTINUOUS_PICTURE;

static struct
{
  jclass klass;
  jmethodID iterator;
} java_util_list = {
  0
};

static struct
{
  jclass klass;
  jmethodID hasNext;
  jmethodID next;
} java_util_iterator = {
  0
};

static struct
{
  jclass klass;
  jmethodID intValue;
} java_lang_integer = {
  0
};

static struct
{
  jclass klass;
  jmethodID equals;
} java_lang_string = {
  0
};

static struct
{
  jclass klass;
  jmethodID constructor;
} org_freedesktop_gstreamer_androidmedia_gstahccallback = {
  0
};

static void
gst_ah_camera_on_preview_frame (JNIEnv * env, jclass klass, jbyteArray data,
    jobject camera, jlong callback, jlong user_data)
{
  GstAHCPreviewCallback cb = (GstAHCPreviewCallback) (gsize) callback;

  if (cb)
    cb (data, (gpointer) (gsize) user_data);
}

static void
gst_ah_camera_on_error (JNIEnv * env, jclass klass, jint error,
    jobject camera, jlong callback, jlong user_data)
{
  GstAHCErrorCallback cb = (GstAHCErrorCallback) (gsize) callback;

  if (cb)
    cb (error, (gpointer) (gsize) user_data);
}

static void
gst_ah_camera_on_auto_focus (JNIEnv * env, jclass klass, jboolean success,
    jobject camera, jlong callback, jlong user_data)
{
  GstAHCAutoFocusCallback cb = (GstAHCAutoFocusCallback) (gsize) callback;

  if (cb)
    cb (success, (gpointer) (gsize) user_data);
}

static JNINativeMethod native_methods[] = {
  {(gchar *) "gst_ah_camera_on_preview_frame",
        (gchar *) "([BLandroid/hardware/Camera;JJ)V",
      (void *) gst_ah_camera_on_preview_frame},
  {(gchar *) "gst_ah_camera_on_error",
        (gchar *) "(ILandroid/hardware/Camera;JJ)V",
      (void *) gst_ah_camera_on_error},
  {(gchar *) "gst_ah_camera_on_auto_focus",
        (gchar *) "(ZLandroid/hardware/Camera;JJ)V",
      (void *) gst_ah_camera_on_auto_focus}
};

static gboolean
_init_classes (void)
{
  JNIEnv *env;
  GError *err = NULL;

  jclass klass;
  jfieldID fieldID;

  env = gst_amc_jni_get_env ();

  /* android.hardware.Camera */
  klass = android_hardware_camera.klass =
      gst_amc_jni_get_class (env, &err, "android/hardware/Camera");
  if (!klass)
    goto failed;

  android_hardware_camera.addCallbackBuffer =
      gst_amc_jni_get_method_id (env, &err, klass,
      "addCallbackBuffer", "([B)V");

  android_hardware_camera.autoFocus =
      gst_amc_jni_get_method_id (env, &err, klass,
      "autoFocus", "(Landroid/hardware/Camera$AutoFocusCallback;)V");

  android_hardware_camera.cancelAutoFocus =
      gst_amc_jni_get_method_id (env, &err, klass, "cancelAutoFocus", "()V");

  android_hardware_camera.getCameraInfo =
      gst_amc_jni_get_static_method_id (env, &err, klass,
      "getCameraInfo", "(ILandroid/hardware/Camera$CameraInfo;)V");

  android_hardware_camera.getNumberOfCameras =
      gst_amc_jni_get_static_method_id (env, &err, klass,
      "getNumberOfCameras", "()I");

  android_hardware_camera.getParameters =
      gst_amc_jni_get_method_id (env, &err, klass,
      "getParameters", "()Landroid/hardware/Camera$Parameters;");

  android_hardware_camera.lock =
      gst_amc_jni_get_method_id (env, &err, klass, "lock", "()V");

  android_hardware_camera.open =
      gst_amc_jni_get_static_method_id (env, &err, klass,
      "open", "(I)Landroid/hardware/Camera;");

  android_hardware_camera.reconnect =
      gst_amc_jni_get_method_id (env, &err, klass, "reconnect", "()V");

  android_hardware_camera.release =
      gst_amc_jni_get_method_id (env, &err, klass, "release", "()V");

  android_hardware_camera.setErrorCallback =
      gst_amc_jni_get_method_id (env, &err, klass, "setErrorCallback",
      "(Landroid/hardware/Camera$ErrorCallback;)V");

  android_hardware_camera.setParameters =
      gst_amc_jni_get_method_id (env, &err, klass, "setParameters",
      "(Landroid/hardware/Camera$Parameters;)V");

  android_hardware_camera.setPreviewCallbackWithBuffer =
      gst_amc_jni_get_method_id (env, &err, klass,
      "setPreviewCallbackWithBuffer",
      "(Landroid/hardware/Camera$PreviewCallback;)V");

  android_hardware_camera.setPreviewTexture =
      gst_amc_jni_get_method_id (env, &err, klass,
      "setPreviewTexture", "(Landroid/graphics/SurfaceTexture;)V");

  android_hardware_camera.startPreview =
      gst_amc_jni_get_method_id (env, &err, klass, "startPreview", "()V");

  android_hardware_camera.startSmoothZoom =
      gst_amc_jni_get_method_id (env, &err, klass, "startSmoothZoom", "(I)V");

  android_hardware_camera.stopPreview =
      gst_amc_jni_get_method_id (env, &err, klass, "stopPreview", "()V");

  android_hardware_camera.stopPreview =
      gst_amc_jni_get_method_id (env, &err, klass, "stopPreview", "()V");

  android_hardware_camera.unlock =
      gst_amc_jni_get_method_id (env, &err, klass, "unlock", "()V");

  /* android.hardware.Camera.CameraInfo */
  klass = android_hardware_camera_camerainfo.klass =
      gst_amc_jni_get_class (env, &err, "android/hardware/Camera$CameraInfo");
  if (!klass)
    goto failed;

  android_hardware_camera_camerainfo.constructor =
      gst_amc_jni_get_method_id (env, &err, klass, "<init>", "()V");

  android_hardware_camera_camerainfo.facing =
      gst_amc_jni_get_field_id (env, &err, klass, "facing", "I");

  android_hardware_camera_camerainfo.orientation =
      gst_amc_jni_get_field_id (env, &err, klass, "orientation", "I");


  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "CAMERA_FACING_BACK",
      "I");
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID,
          &android_hardware_camera_camerainfo.CAMERA_FACING_BACK))
    goto failed;
  CameraInfo_CAMERA_FACING_BACK =
      android_hardware_camera_camerainfo.CAMERA_FACING_BACK;

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "CAMERA_FACING_FRONT",
      "I");
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID,
          &android_hardware_camera_camerainfo.CAMERA_FACING_FRONT))
    goto failed;
  CameraInfo_CAMERA_FACING_FRONT =
      android_hardware_camera_camerainfo.CAMERA_FACING_FRONT;

  /* android.hardware.Camera.Size */
  klass = android_hardware_camera_size.klass =
      gst_amc_jni_get_class (env, &err, "android/hardware/Camera$Size");
  if (!klass)
    goto failed;

  android_hardware_camera_size.width =
      gst_amc_jni_get_field_id (env, &err, klass, "width", "I");
  android_hardware_camera_size.height =
      gst_amc_jni_get_field_id (env, &err, klass, "height", "I");

  /* android.hardware.Camera.Parameters */
  klass = android_hardware_camera_parameters.klass =
      gst_amc_jni_get_class (env, &err, "android/hardware/Camera$Parameters");
  if (!klass)
    goto failed;

  android_hardware_camera_parameters.flatten =
      gst_amc_jni_get_method_id (env, &err, klass, "flatten",
      "()Ljava/lang/String;");

  android_hardware_camera_parameters.getAntibanding =
      gst_amc_jni_get_method_id (env, &err, klass, "getAntibanding",
      "()Ljava/lang/String;");

  android_hardware_camera_parameters.getColorEffect =
      gst_amc_jni_get_method_id (env, &err, klass, "getColorEffect",
      "()Ljava/lang/String;");

  android_hardware_camera_parameters.getExposureCompensation =
      gst_amc_jni_get_method_id (env, &err, klass, "getExposureCompensation",
      "()I");

  android_hardware_camera_parameters.getExposureCompensationStep =
      gst_amc_jni_get_method_id (env, &err, klass,
      "getExposureCompensationStep", "()F");

  android_hardware_camera_parameters.getFlashMode =
      gst_amc_jni_get_method_id (env, &err, klass, "getFlashMode",
      "()Ljava/lang/String;");

  android_hardware_camera_parameters.getFocalLength =
      gst_amc_jni_get_method_id (env, &err, klass, "getFocalLength", "()F");

  android_hardware_camera_parameters.getFocusMode =
      gst_amc_jni_get_method_id (env, &err, klass, "getFocusMode",
      "()Ljava/lang/String;");

  android_hardware_camera_parameters.getHorizontalViewAngle =
      gst_amc_jni_get_method_id (env, &err, klass, "getHorizontalViewAngle",
      "()F");

  android_hardware_camera_parameters.getMaxExposureCompensation =
      gst_amc_jni_get_method_id (env, &err, klass, "getMaxExposureCompensation",
      "()I");

  android_hardware_camera_parameters.getMaxZoom =
      gst_amc_jni_get_method_id (env, &err, klass, "getMaxZoom", "()I");

  android_hardware_camera_parameters.getMinExposureCompensation =
      gst_amc_jni_get_method_id (env, &err, klass, "getMinExposureCompensation",
      "()I");

  android_hardware_camera_parameters.getPreviewFormat =
      gst_amc_jni_get_method_id (env, &err, klass, "getPreviewFormat", "()I");

  android_hardware_camera_parameters.getPreviewFpsRange =
      gst_amc_jni_get_method_id (env, &err, klass, "getPreviewFpsRange",
      "([I)V");

  android_hardware_camera_parameters.getPreviewSize =
      gst_amc_jni_get_method_id (env, &err, klass, "getPreviewSize",
      "()Landroid/hardware/Camera$Size;");

  android_hardware_camera_parameters.getSceneMode =
      gst_amc_jni_get_method_id (env, &err, klass, "getSceneMode",
      "()Ljava/lang/String;");

  android_hardware_camera_parameters.getSupportedAntibanding =
      gst_amc_jni_get_method_id (env, &err, klass, "getSupportedAntibanding",
      "()Ljava/util/List;");

  android_hardware_camera_parameters.getSupportedColorEffects =
      gst_amc_jni_get_method_id (env, &err, klass, "getSupportedColorEffects",
      "()Ljava/util/List;");

  android_hardware_camera_parameters.getSupportedFlashModes =
      gst_amc_jni_get_method_id (env, &err, klass, "getSupportedFlashModes",
      "()Ljava/util/List;");

  android_hardware_camera_parameters.getSupportedFocusModes =
      gst_amc_jni_get_method_id (env, &err, klass, "getSupportedFocusModes",
      "()Ljava/util/List;");

  android_hardware_camera_parameters.getSupportedPreviewFormats =
      gst_amc_jni_get_method_id (env, &err, klass, "getSupportedPreviewFormats",
      "()Ljava/util/List;");

  android_hardware_camera_parameters.getSupportedPreviewFpsRange =
      gst_amc_jni_get_method_id (env, &err, klass,
      "getSupportedPreviewFpsRange", "()Ljava/util/List;");

  android_hardware_camera_parameters.getSupportedPreviewSizes =
      gst_amc_jni_get_method_id (env, &err, klass, "getSupportedPreviewSizes",
      "()Ljava/util/List;");

  android_hardware_camera_parameters.getSupportedSceneModes =
      gst_amc_jni_get_method_id (env, &err, klass, "getSupportedSceneModes",
      "()Ljava/util/List;");

  android_hardware_camera_parameters.getSupportedWhiteBalance =
      gst_amc_jni_get_method_id (env, &err, klass, "getSupportedWhiteBalance",
      "()Ljava/util/List;");

  android_hardware_camera_parameters.getVerticalViewAngle =
      gst_amc_jni_get_method_id (env, &err, klass, "getVerticalViewAngle",
      "()F");

  android_hardware_camera_parameters.getVideoStabilization =
      gst_amc_jni_get_method_id (env, &err, klass, "getVideoStabilization",
      "()Z");

  android_hardware_camera_parameters.getWhiteBalance =
      gst_amc_jni_get_method_id (env, &err, klass, "getWhiteBalance",
      "()Ljava/lang/String;");

  android_hardware_camera_parameters.getZoom =
      gst_amc_jni_get_method_id (env, &err, klass, "getZoom", "()I");

  android_hardware_camera_parameters.getZoomRatios =
      gst_amc_jni_get_method_id (env, &err, klass, "getZoomRatios",
      "()Ljava/util/List;");

  android_hardware_camera_parameters.isSmoothZoomSupported =
      gst_amc_jni_get_method_id (env, &err, klass, "isSmoothZoomSupported",
      "()Z");

  android_hardware_camera_parameters.isVideoStabilizationSupported =
      gst_amc_jni_get_method_id (env, &err, klass,
      "isVideoStabilizationSupported", "()Z");

  android_hardware_camera_parameters.isZoomSupported =
      gst_amc_jni_get_method_id (env, &err, klass, "isZoomSupported", "()Z");

  android_hardware_camera_parameters.setAntibanding =
      gst_amc_jni_get_method_id (env, &err, klass, "setAntibanding",
      "(Ljava/lang/String;)V");

  android_hardware_camera_parameters.setColorEffect =
      gst_amc_jni_get_method_id (env, &err, klass, "setColorEffect",
      "(Ljava/lang/String;)V");

  android_hardware_camera_parameters.setExposureCompensation =
      gst_amc_jni_get_method_id (env, &err, klass, "setExposureCompensation",
      "(I)V");

  android_hardware_camera_parameters.setFlashMode =
      gst_amc_jni_get_method_id (env, &err, klass, "setFlashMode",
      "(Ljava/lang/String;)V");

  android_hardware_camera_parameters.setFocusMode =
      gst_amc_jni_get_method_id (env, &err, klass, "setFocusMode",
      "(Ljava/lang/String;)V");

  android_hardware_camera_parameters.setPreviewFormat =
      gst_amc_jni_get_method_id (env, &err, klass, "setPreviewFormat", "(I)V");

  android_hardware_camera_parameters.setPreviewFpsRange =
      gst_amc_jni_get_method_id (env, &err, klass, "setPreviewFpsRange",
      "(II)V");

  android_hardware_camera_parameters.setPreviewSize =
      gst_amc_jni_get_method_id (env, &err, klass, "setPreviewSize", "(II)V");

  android_hardware_camera_parameters.setSceneMode =
      gst_amc_jni_get_method_id (env, &err, klass, "setSceneMode",
      "(Ljava/lang/String;)V");

  android_hardware_camera_parameters.setWhiteBalance =
      gst_amc_jni_get_method_id (env, &err, klass, "setWhiteBalance",
      "(Ljava/lang/String;)V");

  android_hardware_camera_parameters.setVideoStabilization =
      gst_amc_jni_get_method_id (env, &err, klass, "setVideoStabilization",
      "(Z)V");

  android_hardware_camera_parameters.setZoom =
      gst_amc_jni_get_method_id (env, &err, klass, "setZoom", "(I)V");

  android_hardware_camera_parameters.unflatten =
      gst_amc_jni_get_method_id (env, &err, klass, "unflatten",
      "(Ljava/lang/String;)V");

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "WHITE_BALANCE_AUTO",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.WHITE_BALANCE_AUTO))
    goto failed;

  Parameters_WHITE_BALANCE_AUTO =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.WHITE_BALANCE_AUTO, NULL);
  {
    jobject local = android_hardware_camera_parameters.WHITE_BALANCE_AUTO;

    android_hardware_camera_parameters.WHITE_BALANCE_AUTO =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass,
      "WHITE_BALANCE_INCANDESCENT", "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.WHITE_BALANCE_INCANDESCENT))
    goto failed;

  Parameters_WHITE_BALANCE_INCANDESCENT =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.WHITE_BALANCE_INCANDESCENT, NULL);
  {
    jobject local =
        android_hardware_camera_parameters.WHITE_BALANCE_INCANDESCENT;

    android_hardware_camera_parameters.WHITE_BALANCE_INCANDESCENT =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass,
      "WHITE_BALANCE_FLUORESCENT", "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.WHITE_BALANCE_FLUORESCENT))
    goto failed;

  Parameters_WHITE_BALANCE_FLUORESCENT =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.WHITE_BALANCE_FLUORESCENT, NULL);
  {
    jobject local =
        android_hardware_camera_parameters.WHITE_BALANCE_FLUORESCENT;

    android_hardware_camera_parameters.WHITE_BALANCE_FLUORESCENT =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass,
      "WHITE_BALANCE_WARM_FLUORESCENT", "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.WHITE_BALANCE_WARM_FLUORESCENT))
    goto failed;

  Parameters_WHITE_BALANCE_WARM_FLUORESCENT =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.WHITE_BALANCE_WARM_FLUORESCENT, NULL);
  {
    jobject local =
        android_hardware_camera_parameters.WHITE_BALANCE_WARM_FLUORESCENT;

    android_hardware_camera_parameters.WHITE_BALANCE_WARM_FLUORESCENT =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass,
      "WHITE_BALANCE_DAYLIGHT", "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.WHITE_BALANCE_DAYLIGHT))
    goto failed;

  Parameters_WHITE_BALANCE_DAYLIGHT =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.WHITE_BALANCE_DAYLIGHT, NULL);
  {
    jobject local = android_hardware_camera_parameters.WHITE_BALANCE_DAYLIGHT;

    android_hardware_camera_parameters.WHITE_BALANCE_DAYLIGHT =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass,
      "WHITE_BALANCE_CLOUDY_DAYLIGHT", "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.WHITE_BALANCE_CLOUDY_DAYLIGHT))
    goto failed;
  Parameters_WHITE_BALANCE_CLOUDY_DAYLIGHT =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.WHITE_BALANCE_CLOUDY_DAYLIGHT, NULL);
  {
    jobject local =
        android_hardware_camera_parameters.WHITE_BALANCE_CLOUDY_DAYLIGHT;

    android_hardware_camera_parameters.WHITE_BALANCE_CLOUDY_DAYLIGHT =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass,
      "WHITE_BALANCE_TWILIGHT", "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.WHITE_BALANCE_TWILIGHT))
    goto failed;
  Parameters_WHITE_BALANCE_TWILIGHT =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.WHITE_BALANCE_TWILIGHT, NULL);
  {
    jobject local = android_hardware_camera_parameters.WHITE_BALANCE_TWILIGHT;

    android_hardware_camera_parameters.WHITE_BALANCE_TWILIGHT =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "WHITE_BALANCE_SHADE",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.WHITE_BALANCE_SHADE))
    goto failed;

  Parameters_WHITE_BALANCE_SHADE =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.WHITE_BALANCE_SHADE, NULL);
  {
    jobject local = android_hardware_camera_parameters.WHITE_BALANCE_SHADE;

    android_hardware_camera_parameters.WHITE_BALANCE_SHADE =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "EFFECT_NONE",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.EFFECT_NONE))
    goto failed;

  Parameters_EFFECT_NONE =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.EFFECT_NONE, NULL);
  {
    jobject local = android_hardware_camera_parameters.EFFECT_NONE;

    android_hardware_camera_parameters.EFFECT_NONE =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "EFFECT_MONO",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.EFFECT_MONO))
    goto failed;

  Parameters_EFFECT_MONO =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.EFFECT_MONO, NULL);
  {
    jobject local = android_hardware_camera_parameters.EFFECT_MONO;

    android_hardware_camera_parameters.EFFECT_MONO =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "EFFECT_NEGATIVE",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.EFFECT_NEGATIVE))
    goto failed;

  Parameters_EFFECT_NEGATIVE =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.EFFECT_NEGATIVE, NULL);
  {
    jobject local = android_hardware_camera_parameters.EFFECT_NEGATIVE;

    android_hardware_camera_parameters.EFFECT_NEGATIVE =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "EFFECT_SOLARIZE",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.EFFECT_SOLARIZE))
    goto failed;

  Parameters_EFFECT_SOLARIZE =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.EFFECT_SOLARIZE, NULL);
  {
    jobject local = android_hardware_camera_parameters.EFFECT_SOLARIZE;

    android_hardware_camera_parameters.EFFECT_SOLARIZE =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "EFFECT_SEPIA",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.EFFECT_SEPIA))
    goto failed;

  Parameters_EFFECT_SEPIA =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.EFFECT_SEPIA, NULL);
  {
    jobject local = android_hardware_camera_parameters.EFFECT_SEPIA;

    android_hardware_camera_parameters.EFFECT_SEPIA =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "EFFECT_POSTERIZE",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.EFFECT_POSTERIZE))
    goto failed;

  Parameters_EFFECT_POSTERIZE =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.EFFECT_POSTERIZE, NULL);
  {
    jobject local = android_hardware_camera_parameters.EFFECT_POSTERIZE;

    android_hardware_camera_parameters.EFFECT_POSTERIZE =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "EFFECT_WHITEBOARD",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.EFFECT_WHITEBOARD))
    goto failed;

  Parameters_EFFECT_WHITEBOARD =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.EFFECT_WHITEBOARD, NULL);
  {
    jobject local = android_hardware_camera_parameters.EFFECT_WHITEBOARD;

    android_hardware_camera_parameters.EFFECT_WHITEBOARD =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "EFFECT_BLACKBOARD",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.EFFECT_BLACKBOARD))
    goto failed;

  Parameters_EFFECT_BLACKBOARD =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.EFFECT_BLACKBOARD, NULL);
  {
    jobject local = android_hardware_camera_parameters.EFFECT_BLACKBOARD;

    android_hardware_camera_parameters.EFFECT_BLACKBOARD =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "EFFECT_AQUA",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.EFFECT_AQUA))
    goto failed;

  Parameters_EFFECT_AQUA =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.EFFECT_AQUA, NULL);
  {
    jobject local = android_hardware_camera_parameters.EFFECT_AQUA;

    android_hardware_camera_parameters.EFFECT_AQUA =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "EFFECT_EMBOSS",
      "Ljava/lang/String;");
  if (fieldID) {
    if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
            &android_hardware_camera_parameters.EFFECT_EMBOSS))
      goto failed;

    Parameters_EFFECT_EMBOSS =
        (*env)->GetStringUTFChars (env,
        android_hardware_camera_parameters.EFFECT_EMBOSS, NULL);
    {
      jobject local = android_hardware_camera_parameters.EFFECT_EMBOSS;

      android_hardware_camera_parameters.EFFECT_EMBOSS =
          gst_amc_jni_object_make_global (env, local);
    }
  } else {
    android_hardware_camera_parameters.EFFECT_EMBOSS = NULL;
    g_clear_error (&err);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "EFFECT_SKETCH",
      "Ljava/lang/String;");
  if (fieldID) {
    if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
            &android_hardware_camera_parameters.EFFECT_SKETCH))
      goto failed;

    Parameters_EFFECT_SKETCH =
        (*env)->GetStringUTFChars (env,
        android_hardware_camera_parameters.EFFECT_SKETCH, NULL);
    {
      jobject local = android_hardware_camera_parameters.EFFECT_SKETCH;

      android_hardware_camera_parameters.EFFECT_SKETCH =
          gst_amc_jni_object_make_global (env, local);
    }
  } else {
    android_hardware_camera_parameters.EFFECT_SKETCH = NULL;
    g_clear_error (&err);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "EFFECT_NEON",
      "Ljava/lang/String;");
  if (fieldID) {
    if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
            &android_hardware_camera_parameters.EFFECT_NEON))
      goto failed;

    Parameters_EFFECT_NEON =
        (*env)->GetStringUTFChars (env,
        android_hardware_camera_parameters.EFFECT_NEON, NULL);
    {
      jobject local = android_hardware_camera_parameters.EFFECT_NEON;

      android_hardware_camera_parameters.EFFECT_NEON =
          gst_amc_jni_object_make_global (env, local);
    }
  } else {
    android_hardware_camera_parameters.EFFECT_NEON = NULL;
    g_clear_error (&err);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "ANTIBANDING_AUTO",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.ANTIBANDING_AUTO))
    goto failed;

  Parameters_ANTIBANDING_AUTO =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.ANTIBANDING_AUTO, NULL);
  {
    jobject local = android_hardware_camera_parameters.ANTIBANDING_AUTO;

    android_hardware_camera_parameters.ANTIBANDING_AUTO =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "ANTIBANDING_50HZ",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.ANTIBANDING_50HZ))
    goto failed;

  Parameters_ANTIBANDING_50HZ =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.ANTIBANDING_50HZ, NULL);
  {
    jobject local = android_hardware_camera_parameters.ANTIBANDING_50HZ;

    android_hardware_camera_parameters.ANTIBANDING_50HZ =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "ANTIBANDING_60HZ",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.ANTIBANDING_60HZ))
    goto failed;

  Parameters_ANTIBANDING_60HZ =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.ANTIBANDING_60HZ, NULL);
  {
    jobject local = android_hardware_camera_parameters.ANTIBANDING_60HZ;

    android_hardware_camera_parameters.ANTIBANDING_60HZ =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "ANTIBANDING_OFF",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.ANTIBANDING_OFF))
    goto failed;

  Parameters_ANTIBANDING_OFF =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.ANTIBANDING_OFF, NULL);
  {
    jobject local = android_hardware_camera_parameters.ANTIBANDING_OFF;

    android_hardware_camera_parameters.ANTIBANDING_OFF =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "FLASH_MODE_OFF",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.FLASH_MODE_OFF))
    goto failed;

  Parameters_FLASH_MODE_OFF =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.FLASH_MODE_OFF, NULL);
  {
    jobject local = android_hardware_camera_parameters.FLASH_MODE_OFF;

    android_hardware_camera_parameters.FLASH_MODE_OFF =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "FLASH_MODE_AUTO",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.FLASH_MODE_AUTO))
    goto failed;

  Parameters_FLASH_MODE_AUTO =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.FLASH_MODE_AUTO, NULL);
  {
    jobject local = android_hardware_camera_parameters.FLASH_MODE_AUTO;

    android_hardware_camera_parameters.FLASH_MODE_AUTO =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "FLASH_MODE_ON",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.FLASH_MODE_ON))
    goto failed;

  Parameters_FLASH_MODE_ON =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.FLASH_MODE_ON, NULL);
  {
    jobject local = android_hardware_camera_parameters.FLASH_MODE_ON;

    android_hardware_camera_parameters.FLASH_MODE_ON =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "FLASH_MODE_RED_EYE",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.FLASH_MODE_RED_EYE))
    goto failed;

  Parameters_FLASH_MODE_RED_EYE =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.FLASH_MODE_RED_EYE, NULL);
  {
    jobject local = android_hardware_camera_parameters.FLASH_MODE_RED_EYE;

    android_hardware_camera_parameters.FLASH_MODE_RED_EYE =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "FLASH_MODE_TORCH",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.FLASH_MODE_TORCH))
    goto failed;

  Parameters_FLASH_MODE_TORCH =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.FLASH_MODE_TORCH, NULL);
  {
    jobject local = android_hardware_camera_parameters.FLASH_MODE_TORCH;

    android_hardware_camera_parameters.FLASH_MODE_TORCH =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SCENE_MODE_AUTO",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.SCENE_MODE_AUTO))
    goto failed;

  Parameters_SCENE_MODE_AUTO =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.SCENE_MODE_AUTO, NULL);
  {
    jobject local = android_hardware_camera_parameters.SCENE_MODE_AUTO;

    android_hardware_camera_parameters.SCENE_MODE_AUTO =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SCENE_MODE_ACTION",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.SCENE_MODE_ACTION))
    goto failed;

  Parameters_SCENE_MODE_ACTION =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.SCENE_MODE_ACTION, NULL);
  {
    jobject local = android_hardware_camera_parameters.SCENE_MODE_ACTION;

    android_hardware_camera_parameters.SCENE_MODE_ACTION =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SCENE_MODE_PORTRAIT",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.SCENE_MODE_PORTRAIT))
    goto failed;

  Parameters_SCENE_MODE_PORTRAIT =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.SCENE_MODE_PORTRAIT, NULL);
  {
    jobject local = android_hardware_camera_parameters.SCENE_MODE_PORTRAIT;

    android_hardware_camera_parameters.SCENE_MODE_PORTRAIT =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SCENE_MODE_LANDSCAPE",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.SCENE_MODE_LANDSCAPE))
    goto failed;
  Parameters_SCENE_MODE_LANDSCAPE =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.SCENE_MODE_LANDSCAPE, NULL);
  {
    jobject local = android_hardware_camera_parameters.SCENE_MODE_LANDSCAPE;

    android_hardware_camera_parameters.SCENE_MODE_LANDSCAPE =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SCENE_MODE_NIGHT",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.SCENE_MODE_NIGHT))
    goto failed;

  Parameters_SCENE_MODE_NIGHT =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.SCENE_MODE_NIGHT, NULL);
  {
    jobject local = android_hardware_camera_parameters.SCENE_MODE_NIGHT;

    android_hardware_camera_parameters.SCENE_MODE_NIGHT =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass,
      "SCENE_MODE_NIGHT_PORTRAIT", "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.SCENE_MODE_NIGHT_PORTRAIT))
    goto failed;
  Parameters_SCENE_MODE_NIGHT_PORTRAIT =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.SCENE_MODE_NIGHT_PORTRAIT, NULL);
  {
    jobject local =
        android_hardware_camera_parameters.SCENE_MODE_NIGHT_PORTRAIT;

    android_hardware_camera_parameters.SCENE_MODE_NIGHT_PORTRAIT =
        gst_amc_jni_object_make_global (env, local);
  }
  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SCENE_MODE_THEATRE",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.SCENE_MODE_THEATRE))
    goto failed;

  Parameters_SCENE_MODE_THEATRE =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.SCENE_MODE_THEATRE, NULL);
  {
    jobject local = android_hardware_camera_parameters.SCENE_MODE_THEATRE;

    android_hardware_camera_parameters.SCENE_MODE_THEATRE =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SCENE_MODE_BEACH",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.SCENE_MODE_BEACH))
    goto failed;

  Parameters_SCENE_MODE_BEACH =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.SCENE_MODE_BEACH, NULL);
  {
    jobject local = android_hardware_camera_parameters.SCENE_MODE_BEACH;

    android_hardware_camera_parameters.SCENE_MODE_BEACH =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SCENE_MODE_SNOW",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.SCENE_MODE_SNOW))
    goto failed;

  Parameters_SCENE_MODE_SNOW =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.SCENE_MODE_SNOW, NULL);
  {
    jobject local = android_hardware_camera_parameters.SCENE_MODE_SNOW;

    android_hardware_camera_parameters.SCENE_MODE_SNOW =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SCENE_MODE_SUNSET",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.SCENE_MODE_SUNSET))
    goto failed;


  Parameters_SCENE_MODE_SUNSET =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.SCENE_MODE_SUNSET, NULL);
  {
    jobject local = android_hardware_camera_parameters.SCENE_MODE_SUNSET;

    android_hardware_camera_parameters.SCENE_MODE_SUNSET =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass,
      "SCENE_MODE_STEADYPHOTO", "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.SCENE_MODE_STEADYPHOTO))
    goto failed;


  Parameters_SCENE_MODE_STEADYPHOTO =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.SCENE_MODE_STEADYPHOTO, NULL);
  {
    jobject local = android_hardware_camera_parameters.SCENE_MODE_STEADYPHOTO;

    android_hardware_camera_parameters.SCENE_MODE_STEADYPHOTO =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SCENE_MODE_FIREWORKS",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.SCENE_MODE_FIREWORKS))
    goto failed;

  Parameters_SCENE_MODE_FIREWORKS =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.SCENE_MODE_FIREWORKS, NULL);
  {
    jobject local = android_hardware_camera_parameters.SCENE_MODE_FIREWORKS;

    android_hardware_camera_parameters.SCENE_MODE_FIREWORKS =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SCENE_MODE_SPORTS",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.SCENE_MODE_SPORTS))
    goto failed;


  Parameters_SCENE_MODE_SPORTS =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.SCENE_MODE_SPORTS, NULL);
  {
    jobject local = android_hardware_camera_parameters.SCENE_MODE_SPORTS;

    android_hardware_camera_parameters.SCENE_MODE_SPORTS =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SCENE_MODE_PARTY",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.SCENE_MODE_PARTY))
    goto failed;

  Parameters_SCENE_MODE_PARTY =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.SCENE_MODE_PARTY, NULL);
  {
    jobject local = android_hardware_camera_parameters.SCENE_MODE_PARTY;

    android_hardware_camera_parameters.SCENE_MODE_PARTY =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass,
      "SCENE_MODE_CANDLELIGHT", "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.SCENE_MODE_CANDLELIGHT))
    goto failed;

  Parameters_SCENE_MODE_CANDLELIGHT =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.SCENE_MODE_CANDLELIGHT, NULL);
  {
    jobject local = android_hardware_camera_parameters.SCENE_MODE_CANDLELIGHT;

    android_hardware_camera_parameters.SCENE_MODE_CANDLELIGHT =
        gst_amc_jni_object_make_global (env, local);
  }


  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SCENE_MODE_BARCODE",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.SCENE_MODE_BARCODE))
    goto failed;

  Parameters_SCENE_MODE_BARCODE =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.SCENE_MODE_BARCODE, NULL);
  {
    jobject local = android_hardware_camera_parameters.SCENE_MODE_BARCODE;

    android_hardware_camera_parameters.SCENE_MODE_BARCODE =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SCENE_MODE_BACKLIGHT",
      "Ljava/lang/String;");
  if (fieldID) {
    if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
            &android_hardware_camera_parameters.SCENE_MODE_BACKLIGHT))
      goto failed;

    Parameters_SCENE_MODE_BACKLIGHT =
        (*env)->GetStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_BACKLIGHT, NULL);
    {
      jobject local = android_hardware_camera_parameters.SCENE_MODE_BACKLIGHT;

      android_hardware_camera_parameters.SCENE_MODE_BACKLIGHT =
          gst_amc_jni_object_make_global (env, local);
    }
  } else {
    android_hardware_camera_parameters.SCENE_MODE_BACKLIGHT = NULL;
    g_clear_error (&err);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SCENE_MODE_FLOWERS",
      "Ljava/lang/String;");
  if (fieldID) {
    if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
            &android_hardware_camera_parameters.SCENE_MODE_FLOWERS))
      goto failed;

    Parameters_SCENE_MODE_FLOWERS =
        (*env)->GetStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_FLOWERS, NULL);
    {
      jobject local = android_hardware_camera_parameters.SCENE_MODE_FLOWERS;

      android_hardware_camera_parameters.SCENE_MODE_FLOWERS =
          gst_amc_jni_object_make_global (env, local);
    }
  } else {
    android_hardware_camera_parameters.SCENE_MODE_FLOWERS = NULL;
    g_clear_error (&err);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SCENE_MODE_AR",
      "Ljava/lang/String;");
  if (fieldID) {
    if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
            &android_hardware_camera_parameters.SCENE_MODE_AR))
      goto failed;

    Parameters_SCENE_MODE_AR =
        (*env)->GetStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_AR, NULL);
    {
      jobject local = android_hardware_camera_parameters.SCENE_MODE_AR;

      android_hardware_camera_parameters.SCENE_MODE_AR =
          gst_amc_jni_object_make_global (env, local);
    }
  } else {
    android_hardware_camera_parameters.SCENE_MODE_AR = NULL;
    g_clear_error (&err);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SCENE_MODE_HDR",
      "Ljava/lang/String;");
  if (fieldID) {
    if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
            &android_hardware_camera_parameters.SCENE_MODE_HDR))
      goto failed;

    Parameters_SCENE_MODE_HDR =
        (*env)->GetStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_HDR, NULL);
    {
      jobject local = android_hardware_camera_parameters.SCENE_MODE_HDR;

      android_hardware_camera_parameters.SCENE_MODE_HDR =
          gst_amc_jni_object_make_global (env, local);
    }
  } else {
    android_hardware_camera_parameters.SCENE_MODE_HDR = NULL;
    g_clear_error (&err);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "FOCUS_MODE_AUTO",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.FOCUS_MODE_AUTO))
    goto failed;

  Parameters_FOCUS_MODE_AUTO =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.FOCUS_MODE_AUTO, NULL);
  {
    jobject local = android_hardware_camera_parameters.FOCUS_MODE_AUTO;

    android_hardware_camera_parameters.FOCUS_MODE_AUTO =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "FOCUS_MODE_INFINITY",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.FOCUS_MODE_INFINITY))
    goto failed;

  Parameters_FOCUS_MODE_INFINITY =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.FOCUS_MODE_INFINITY, NULL);
  {
    jobject local = android_hardware_camera_parameters.FOCUS_MODE_INFINITY;

    android_hardware_camera_parameters.FOCUS_MODE_INFINITY =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "FOCUS_MODE_MACRO",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.FOCUS_MODE_MACRO))
    goto failed;

  Parameters_FOCUS_MODE_MACRO =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.FOCUS_MODE_MACRO, NULL);
  {
    jobject local = android_hardware_camera_parameters.FOCUS_MODE_MACRO;

    android_hardware_camera_parameters.FOCUS_MODE_MACRO =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "FOCUS_MODE_FIXED",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.FOCUS_MODE_FIXED))
    goto failed;

  Parameters_FOCUS_MODE_FIXED =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.FOCUS_MODE_FIXED, NULL);
  {
    jobject local = android_hardware_camera_parameters.FOCUS_MODE_FIXED;

    android_hardware_camera_parameters.FOCUS_MODE_FIXED =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "FOCUS_MODE_EDOF",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.FOCUS_MODE_EDOF))
    goto failed;

  Parameters_FOCUS_MODE_EDOF =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.FOCUS_MODE_EDOF, NULL);
  {
    jobject local = android_hardware_camera_parameters.FOCUS_MODE_EDOF;

    android_hardware_camera_parameters.FOCUS_MODE_EDOF =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass,
      "FOCUS_MODE_CONTINUOUS_VIDEO", "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.FOCUS_MODE_CONTINUOUS_VIDEO))
    goto failed;

  Parameters_FOCUS_MODE_CONTINUOUS_VIDEO =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.FOCUS_MODE_CONTINUOUS_VIDEO, NULL);
  {
    jobject local =
        android_hardware_camera_parameters.FOCUS_MODE_CONTINUOUS_VIDEO;

    android_hardware_camera_parameters.FOCUS_MODE_CONTINUOUS_VIDEO =
        gst_amc_jni_object_make_global (env, local);
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass,
      "FOCUS_MODE_CONTINUOUS_PICTURE", "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_hardware_camera_parameters.FOCUS_MODE_CONTINUOUS_PICTURE))
    goto failed;


  Parameters_FOCUS_MODE_CONTINUOUS_PICTURE =
      (*env)->GetStringUTFChars (env,
      android_hardware_camera_parameters.FOCUS_MODE_CONTINUOUS_PICTURE, NULL);
  {
    jobject local =
        android_hardware_camera_parameters.FOCUS_MODE_CONTINUOUS_PICTURE;

    android_hardware_camera_parameters.FOCUS_MODE_CONTINUOUS_PICTURE =
        gst_amc_jni_object_make_global (env, local);
  }

  /* java.lang.String */
  klass = java_lang_string.klass =
      gst_amc_jni_get_class (env, &err, "java/lang/String");
  java_lang_string.equals =
      gst_amc_jni_get_method_id (env, &err, klass, "equals",
      "(Ljava/lang/Object;)Z");

  /* java.util.List */
  klass = java_util_list.klass =
      gst_amc_jni_get_class (env, &err, "java/util/List");
  java_util_list.iterator =
      gst_amc_jni_get_method_id (env, &err, klass, "iterator",
      "()Ljava/util/Iterator;");

  /* java.util.Iterator */
  klass = java_util_iterator.klass =
      gst_amc_jni_get_class (env, &err, "java/util/Iterator");
  java_util_iterator.hasNext =
      gst_amc_jni_get_method_id (env, &err, klass, "hasNext", "()Z");
  java_util_iterator.next =
      gst_amc_jni_get_method_id (env, &err, klass, "next",
      "()Ljava/lang/Object;");

  /* java.lang.Integer */
  klass = java_lang_integer.klass =
      gst_amc_jni_get_class (env, &err, "java/lang/Integer");
  java_lang_integer.intValue =
      gst_amc_jni_get_method_id (env, &err, klass, "intValue", "()I");

  if (!org_freedesktop_gstreamer_androidmedia_gstahccallback.klass) {
    org_freedesktop_gstreamer_androidmedia_gstahccallback.klass =
        gst_amc_jni_get_application_class (env,
        "org/freedesktop/gstreamer/androidmedia/GstAhcCallback", &err);
  }
  if (!org_freedesktop_gstreamer_androidmedia_gstahccallback.klass)
    goto failed;

  org_freedesktop_gstreamer_androidmedia_gstahccallback.constructor =
      gst_amc_jni_get_method_id (env, &err,
      org_freedesktop_gstreamer_androidmedia_gstahccallback.klass, "<init>",
      "(JJ)V");

  if ((*env)->RegisterNatives (env,
          org_freedesktop_gstreamer_androidmedia_gstahccallback.klass,
          native_methods, G_N_ELEMENTS (native_methods))) {
    GST_ERROR ("Failed to register native methods for GstAhcCallback");
    return FALSE;
  }

  return TRUE;

failed:
  if (err) {
    GST_ERROR ("Failed to initialize android.hardware.Camera classes: %s",
        err->message);
    g_clear_error (&err);
  }

  return FALSE;

}


gboolean
gst_android_hardware_camera_init (void)
{
  GST_DEBUG_CATEGORY_INIT (ahc_debug, "ahc", 0,
      "Android Gstreamer Hardware Camera");
  if (!_init_classes ()) {
    gst_android_hardware_camera_deinit ();
    return FALSE;
  }

  return TRUE;
}

void
gst_android_hardware_camera_deinit (void)
{
  JNIEnv *env = gst_amc_jni_get_env ();

  if (android_hardware_camera.klass)
    gst_amc_jni_object_unref (env, android_hardware_camera.klass);
  android_hardware_camera.klass = NULL;

  if (android_hardware_camera_camerainfo.klass)
    gst_amc_jni_object_unref (env, android_hardware_camera_camerainfo.klass);
  android_hardware_camera_camerainfo.klass = NULL;

  if (android_hardware_camera_size.klass)
    gst_amc_jni_object_unref (env, android_hardware_camera_size.klass);
  android_hardware_camera_size.klass = NULL;

  if (android_hardware_camera_parameters.klass)
    gst_amc_jni_object_unref (env, android_hardware_camera_parameters.klass);
  android_hardware_camera_parameters.klass = NULL;
  if (Parameters_WHITE_BALANCE_AUTO)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.WHITE_BALANCE_AUTO,
        Parameters_WHITE_BALANCE_AUTO);
  Parameters_WHITE_BALANCE_AUTO = NULL;
  if (android_hardware_camera_parameters.WHITE_BALANCE_AUTO)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.WHITE_BALANCE_AUTO);
  android_hardware_camera_parameters.WHITE_BALANCE_AUTO = NULL;
  if (Parameters_WHITE_BALANCE_INCANDESCENT)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.WHITE_BALANCE_INCANDESCENT,
        Parameters_WHITE_BALANCE_INCANDESCENT);
  Parameters_WHITE_BALANCE_INCANDESCENT = NULL;
  if (android_hardware_camera_parameters.WHITE_BALANCE_INCANDESCENT)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.WHITE_BALANCE_INCANDESCENT);
  android_hardware_camera_parameters.WHITE_BALANCE_INCANDESCENT = NULL;
  if (Parameters_WHITE_BALANCE_FLUORESCENT)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.WHITE_BALANCE_FLUORESCENT,
        Parameters_WHITE_BALANCE_FLUORESCENT);
  Parameters_WHITE_BALANCE_FLUORESCENT = NULL;
  if (android_hardware_camera_parameters.WHITE_BALANCE_FLUORESCENT)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.WHITE_BALANCE_FLUORESCENT);
  android_hardware_camera_parameters.WHITE_BALANCE_FLUORESCENT = NULL;
  if (Parameters_WHITE_BALANCE_WARM_FLUORESCENT)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.WHITE_BALANCE_WARM_FLUORESCENT,
        Parameters_WHITE_BALANCE_WARM_FLUORESCENT);
  Parameters_WHITE_BALANCE_WARM_FLUORESCENT = NULL;
  if (android_hardware_camera_parameters.WHITE_BALANCE_WARM_FLUORESCENT)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.WHITE_BALANCE_WARM_FLUORESCENT);
  android_hardware_camera_parameters.WHITE_BALANCE_WARM_FLUORESCENT = NULL;
  if (Parameters_WHITE_BALANCE_DAYLIGHT)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.WHITE_BALANCE_DAYLIGHT,
        Parameters_WHITE_BALANCE_DAYLIGHT);
  Parameters_WHITE_BALANCE_DAYLIGHT = NULL;
  if (android_hardware_camera_parameters.WHITE_BALANCE_DAYLIGHT)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.WHITE_BALANCE_DAYLIGHT);
  android_hardware_camera_parameters.WHITE_BALANCE_DAYLIGHT = NULL;
  if (Parameters_WHITE_BALANCE_CLOUDY_DAYLIGHT)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.WHITE_BALANCE_CLOUDY_DAYLIGHT,
        Parameters_WHITE_BALANCE_CLOUDY_DAYLIGHT);
  Parameters_WHITE_BALANCE_CLOUDY_DAYLIGHT = NULL;
  if (android_hardware_camera_parameters.WHITE_BALANCE_CLOUDY_DAYLIGHT)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.WHITE_BALANCE_CLOUDY_DAYLIGHT);
  android_hardware_camera_parameters.WHITE_BALANCE_CLOUDY_DAYLIGHT = NULL;
  if (Parameters_WHITE_BALANCE_TWILIGHT)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.WHITE_BALANCE_TWILIGHT,
        Parameters_WHITE_BALANCE_TWILIGHT);
  Parameters_WHITE_BALANCE_TWILIGHT = NULL;
  if (android_hardware_camera_parameters.WHITE_BALANCE_TWILIGHT)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.WHITE_BALANCE_TWILIGHT);
  android_hardware_camera_parameters.WHITE_BALANCE_TWILIGHT = NULL;
  if (Parameters_WHITE_BALANCE_SHADE)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.WHITE_BALANCE_SHADE,
        Parameters_WHITE_BALANCE_SHADE);
  Parameters_WHITE_BALANCE_SHADE = NULL;
  if (android_hardware_camera_parameters.WHITE_BALANCE_SHADE)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.WHITE_BALANCE_SHADE);
  android_hardware_camera_parameters.WHITE_BALANCE_SHADE = NULL;
  if (Parameters_EFFECT_NONE)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.EFFECT_NONE, Parameters_EFFECT_NONE);
  Parameters_EFFECT_NONE = NULL;
  if (android_hardware_camera_parameters.EFFECT_NONE)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.EFFECT_NONE);
  android_hardware_camera_parameters.EFFECT_NONE = NULL;
  if (Parameters_EFFECT_MONO)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.EFFECT_MONO, Parameters_EFFECT_MONO);
  Parameters_EFFECT_MONO = NULL;
  if (android_hardware_camera_parameters.EFFECT_MONO)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.EFFECT_MONO);
  android_hardware_camera_parameters.EFFECT_MONO = NULL;
  if (Parameters_EFFECT_NEGATIVE)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.EFFECT_NEGATIVE,
        Parameters_EFFECT_NEGATIVE);
  Parameters_EFFECT_NEGATIVE = NULL;
  if (android_hardware_camera_parameters.EFFECT_NEGATIVE)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.EFFECT_NEGATIVE);
  android_hardware_camera_parameters.EFFECT_NEGATIVE = NULL;
  if (Parameters_EFFECT_SOLARIZE)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.EFFECT_SOLARIZE,
        Parameters_EFFECT_SOLARIZE);
  Parameters_EFFECT_SOLARIZE = NULL;
  if (android_hardware_camera_parameters.EFFECT_SOLARIZE)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.EFFECT_SOLARIZE);
  android_hardware_camera_parameters.EFFECT_SOLARIZE = NULL;
  if (Parameters_EFFECT_SEPIA)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.EFFECT_SEPIA,
        Parameters_EFFECT_SEPIA);
  Parameters_EFFECT_SEPIA = NULL;
  if (android_hardware_camera_parameters.EFFECT_SEPIA)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.EFFECT_SEPIA);
  android_hardware_camera_parameters.EFFECT_SEPIA = NULL;
  if (Parameters_EFFECT_POSTERIZE)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.EFFECT_POSTERIZE,
        Parameters_EFFECT_POSTERIZE);
  Parameters_EFFECT_POSTERIZE = NULL;
  if (android_hardware_camera_parameters.EFFECT_POSTERIZE)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.EFFECT_POSTERIZE);
  android_hardware_camera_parameters.EFFECT_POSTERIZE = NULL;
  if (Parameters_EFFECT_WHITEBOARD)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.EFFECT_WHITEBOARD,
        Parameters_EFFECT_WHITEBOARD);
  Parameters_EFFECT_WHITEBOARD = NULL;
  if (android_hardware_camera_parameters.EFFECT_WHITEBOARD)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.EFFECT_WHITEBOARD);
  android_hardware_camera_parameters.EFFECT_WHITEBOARD = NULL;
  if (Parameters_EFFECT_BLACKBOARD)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.EFFECT_BLACKBOARD,
        Parameters_EFFECT_BLACKBOARD);
  Parameters_EFFECT_BLACKBOARD = NULL;
  if (android_hardware_camera_parameters.EFFECT_BLACKBOARD)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.EFFECT_BLACKBOARD);
  android_hardware_camera_parameters.EFFECT_BLACKBOARD = NULL;
  if (Parameters_EFFECT_AQUA)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.EFFECT_AQUA, Parameters_EFFECT_AQUA);
  Parameters_EFFECT_AQUA = NULL;
  if (android_hardware_camera_parameters.EFFECT_AQUA)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.EFFECT_AQUA);
  android_hardware_camera_parameters.EFFECT_AQUA = NULL;
  if (Parameters_EFFECT_EMBOSS)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.EFFECT_EMBOSS,
        Parameters_EFFECT_EMBOSS);
  Parameters_EFFECT_EMBOSS = NULL;
  if (android_hardware_camera_parameters.EFFECT_EMBOSS)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.EFFECT_EMBOSS);
  android_hardware_camera_parameters.EFFECT_EMBOSS = NULL;
  if (Parameters_EFFECT_SKETCH)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.EFFECT_SKETCH,
        Parameters_EFFECT_SKETCH);
  Parameters_EFFECT_SKETCH = NULL;
  if (android_hardware_camera_parameters.EFFECT_SKETCH)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.EFFECT_SKETCH);
  android_hardware_camera_parameters.EFFECT_SKETCH = NULL;
  if (Parameters_EFFECT_NEON)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.EFFECT_NEON, Parameters_EFFECT_NEON);
  Parameters_EFFECT_NEON = NULL;
  if (android_hardware_camera_parameters.EFFECT_NEON)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.EFFECT_NEON);
  android_hardware_camera_parameters.EFFECT_NEON = NULL;
  if (Parameters_ANTIBANDING_AUTO)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.ANTIBANDING_AUTO,
        Parameters_ANTIBANDING_AUTO);
  Parameters_ANTIBANDING_AUTO = NULL;
  if (android_hardware_camera_parameters.ANTIBANDING_AUTO)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.ANTIBANDING_AUTO);
  android_hardware_camera_parameters.ANTIBANDING_AUTO = NULL;
  if (Parameters_ANTIBANDING_50HZ)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.ANTIBANDING_50HZ,
        Parameters_ANTIBANDING_50HZ);
  Parameters_ANTIBANDING_50HZ = NULL;
  if (android_hardware_camera_parameters.ANTIBANDING_50HZ)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.ANTIBANDING_50HZ);
  android_hardware_camera_parameters.ANTIBANDING_50HZ = NULL;
  if (Parameters_ANTIBANDING_60HZ)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.ANTIBANDING_60HZ,
        Parameters_ANTIBANDING_60HZ);
  Parameters_ANTIBANDING_60HZ = NULL;
  if (android_hardware_camera_parameters.ANTIBANDING_60HZ)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.ANTIBANDING_60HZ);
  android_hardware_camera_parameters.ANTIBANDING_60HZ = NULL;
  if (Parameters_ANTIBANDING_OFF)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.ANTIBANDING_OFF,
        Parameters_ANTIBANDING_OFF);
  Parameters_ANTIBANDING_OFF = NULL;
  if (android_hardware_camera_parameters.ANTIBANDING_OFF)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.ANTIBANDING_OFF);
  android_hardware_camera_parameters.ANTIBANDING_OFF = NULL;
  if (Parameters_FLASH_MODE_OFF)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.FLASH_MODE_OFF,
        Parameters_FLASH_MODE_OFF);
  Parameters_FLASH_MODE_OFF = NULL;
  if (android_hardware_camera_parameters.FLASH_MODE_OFF)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.FLASH_MODE_OFF);
  android_hardware_camera_parameters.FLASH_MODE_OFF = NULL;
  if (Parameters_FLASH_MODE_AUTO)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.FLASH_MODE_AUTO,
        Parameters_FLASH_MODE_AUTO);
  Parameters_FLASH_MODE_AUTO = NULL;
  if (android_hardware_camera_parameters.FLASH_MODE_AUTO)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.FLASH_MODE_AUTO);
  android_hardware_camera_parameters.FLASH_MODE_AUTO = NULL;
  if (Parameters_FLASH_MODE_ON)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.FLASH_MODE_ON,
        Parameters_FLASH_MODE_ON);
  Parameters_FLASH_MODE_ON = NULL;
  if (android_hardware_camera_parameters.FLASH_MODE_ON)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.FLASH_MODE_ON);
  android_hardware_camera_parameters.FLASH_MODE_ON = NULL;
  if (Parameters_FLASH_MODE_RED_EYE)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.FLASH_MODE_RED_EYE,
        Parameters_FLASH_MODE_RED_EYE);
  Parameters_FLASH_MODE_RED_EYE = NULL;
  if (android_hardware_camera_parameters.FLASH_MODE_RED_EYE)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.FLASH_MODE_RED_EYE);
  android_hardware_camera_parameters.FLASH_MODE_RED_EYE = NULL;
  if (Parameters_FLASH_MODE_TORCH)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.FLASH_MODE_TORCH,
        Parameters_FLASH_MODE_TORCH);
  Parameters_FLASH_MODE_TORCH = NULL;
  if (android_hardware_camera_parameters.FLASH_MODE_TORCH)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.FLASH_MODE_TORCH);
  android_hardware_camera_parameters.FLASH_MODE_TORCH = NULL;
  if (Parameters_SCENE_MODE_AUTO)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_AUTO,
        Parameters_SCENE_MODE_AUTO);
  Parameters_SCENE_MODE_AUTO = NULL;
  if (android_hardware_camera_parameters.SCENE_MODE_AUTO)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.SCENE_MODE_AUTO);
  android_hardware_camera_parameters.SCENE_MODE_AUTO = NULL;
  if (Parameters_SCENE_MODE_ACTION)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_ACTION,
        Parameters_SCENE_MODE_ACTION);
  Parameters_SCENE_MODE_ACTION = NULL;
  if (android_hardware_camera_parameters.SCENE_MODE_ACTION)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.SCENE_MODE_ACTION);
  android_hardware_camera_parameters.SCENE_MODE_ACTION = NULL;
  if (Parameters_SCENE_MODE_PORTRAIT)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_PORTRAIT,
        Parameters_SCENE_MODE_PORTRAIT);
  Parameters_SCENE_MODE_PORTRAIT = NULL;
  if (android_hardware_camera_parameters.SCENE_MODE_PORTRAIT)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.SCENE_MODE_PORTRAIT);
  android_hardware_camera_parameters.SCENE_MODE_PORTRAIT = NULL;
  if (Parameters_SCENE_MODE_LANDSCAPE)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_LANDSCAPE,
        Parameters_SCENE_MODE_LANDSCAPE);
  Parameters_SCENE_MODE_LANDSCAPE = NULL;
  if (android_hardware_camera_parameters.SCENE_MODE_LANDSCAPE)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.SCENE_MODE_LANDSCAPE);
  android_hardware_camera_parameters.SCENE_MODE_LANDSCAPE = NULL;
  if (Parameters_SCENE_MODE_NIGHT)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_NIGHT,
        Parameters_SCENE_MODE_NIGHT);
  Parameters_SCENE_MODE_NIGHT = NULL;
  if (android_hardware_camera_parameters.SCENE_MODE_NIGHT)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.SCENE_MODE_NIGHT);
  android_hardware_camera_parameters.SCENE_MODE_NIGHT = NULL;
  if (Parameters_SCENE_MODE_NIGHT_PORTRAIT)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_NIGHT_PORTRAIT,
        Parameters_SCENE_MODE_NIGHT_PORTRAIT);
  Parameters_SCENE_MODE_NIGHT_PORTRAIT = NULL;
  if (android_hardware_camera_parameters.SCENE_MODE_NIGHT_PORTRAIT)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.SCENE_MODE_NIGHT_PORTRAIT);
  android_hardware_camera_parameters.SCENE_MODE_NIGHT_PORTRAIT = NULL;
  if (Parameters_SCENE_MODE_THEATRE)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_THEATRE,
        Parameters_SCENE_MODE_THEATRE);
  Parameters_SCENE_MODE_THEATRE = NULL;
  if (android_hardware_camera_parameters.SCENE_MODE_THEATRE)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.SCENE_MODE_THEATRE);
  android_hardware_camera_parameters.SCENE_MODE_THEATRE = NULL;
  if (Parameters_SCENE_MODE_BEACH)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_BEACH,
        Parameters_SCENE_MODE_BEACH);
  Parameters_SCENE_MODE_BEACH = NULL;
  if (android_hardware_camera_parameters.SCENE_MODE_BEACH)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.SCENE_MODE_BEACH);
  android_hardware_camera_parameters.SCENE_MODE_BEACH = NULL;
  if (Parameters_SCENE_MODE_SNOW)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_SNOW,
        Parameters_SCENE_MODE_SNOW);
  Parameters_SCENE_MODE_SNOW = NULL;
  if (android_hardware_camera_parameters.SCENE_MODE_SNOW)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.SCENE_MODE_SNOW);
  android_hardware_camera_parameters.SCENE_MODE_SNOW = NULL;
  if (Parameters_SCENE_MODE_SUNSET)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_SUNSET,
        Parameters_SCENE_MODE_SUNSET);
  Parameters_SCENE_MODE_SUNSET = NULL;
  if (android_hardware_camera_parameters.SCENE_MODE_SUNSET)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.SCENE_MODE_SUNSET);
  android_hardware_camera_parameters.SCENE_MODE_SUNSET = NULL;
  if (Parameters_SCENE_MODE_STEADYPHOTO)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_STEADYPHOTO,
        Parameters_SCENE_MODE_STEADYPHOTO);
  Parameters_SCENE_MODE_STEADYPHOTO = NULL;
  if (android_hardware_camera_parameters.SCENE_MODE_STEADYPHOTO)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.SCENE_MODE_STEADYPHOTO);
  android_hardware_camera_parameters.SCENE_MODE_STEADYPHOTO = NULL;
  if (Parameters_SCENE_MODE_FIREWORKS)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_FIREWORKS,
        Parameters_SCENE_MODE_FIREWORKS);
  Parameters_SCENE_MODE_FIREWORKS = NULL;
  if (android_hardware_camera_parameters.SCENE_MODE_FIREWORKS)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.SCENE_MODE_FIREWORKS);
  android_hardware_camera_parameters.SCENE_MODE_FIREWORKS = NULL;
  if (Parameters_SCENE_MODE_SPORTS)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_SPORTS,
        Parameters_SCENE_MODE_SPORTS);
  Parameters_SCENE_MODE_SPORTS = NULL;
  if (android_hardware_camera_parameters.SCENE_MODE_SPORTS)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.SCENE_MODE_SPORTS);
  android_hardware_camera_parameters.SCENE_MODE_SPORTS = NULL;
  if (Parameters_SCENE_MODE_PARTY)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_PARTY,
        Parameters_SCENE_MODE_PARTY);
  Parameters_SCENE_MODE_PARTY = NULL;
  if (android_hardware_camera_parameters.SCENE_MODE_PARTY)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.SCENE_MODE_PARTY);
  android_hardware_camera_parameters.SCENE_MODE_PARTY = NULL;
  if (Parameters_SCENE_MODE_CANDLELIGHT)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_CANDLELIGHT,
        Parameters_SCENE_MODE_CANDLELIGHT);
  Parameters_SCENE_MODE_CANDLELIGHT = NULL;
  if (android_hardware_camera_parameters.SCENE_MODE_CANDLELIGHT)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.SCENE_MODE_CANDLELIGHT);
  android_hardware_camera_parameters.SCENE_MODE_CANDLELIGHT = NULL;
  if (Parameters_SCENE_MODE_BARCODE)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_BARCODE,
        Parameters_SCENE_MODE_BARCODE);
  Parameters_SCENE_MODE_BARCODE = NULL;
  if (android_hardware_camera_parameters.SCENE_MODE_BARCODE)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.SCENE_MODE_BARCODE);
  android_hardware_camera_parameters.SCENE_MODE_BARCODE = NULL;
  if (Parameters_SCENE_MODE_BACKLIGHT)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_BACKLIGHT,
        Parameters_SCENE_MODE_BACKLIGHT);
  Parameters_SCENE_MODE_BACKLIGHT = NULL;
  if (android_hardware_camera_parameters.SCENE_MODE_BACKLIGHT)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.SCENE_MODE_BACKLIGHT);
  android_hardware_camera_parameters.SCENE_MODE_BACKLIGHT = NULL;
  if (Parameters_SCENE_MODE_FLOWERS)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_FLOWERS,
        Parameters_SCENE_MODE_FLOWERS);
  Parameters_SCENE_MODE_FLOWERS = NULL;
  if (android_hardware_camera_parameters.SCENE_MODE_FLOWERS)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.SCENE_MODE_FLOWERS);
  android_hardware_camera_parameters.SCENE_MODE_FLOWERS = NULL;
  if (Parameters_SCENE_MODE_AR)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_AR,
        Parameters_SCENE_MODE_AR);
  Parameters_SCENE_MODE_AR = NULL;
  if (android_hardware_camera_parameters.SCENE_MODE_AR)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.SCENE_MODE_AR);
  android_hardware_camera_parameters.SCENE_MODE_AR = NULL;
  if (Parameters_SCENE_MODE_HDR)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.SCENE_MODE_HDR,
        Parameters_SCENE_MODE_HDR);
  Parameters_SCENE_MODE_HDR = NULL;
  if (android_hardware_camera_parameters.SCENE_MODE_HDR)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.SCENE_MODE_HDR);
  android_hardware_camera_parameters.SCENE_MODE_HDR = NULL;
  if (Parameters_FOCUS_MODE_AUTO)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.FOCUS_MODE_AUTO,
        Parameters_FOCUS_MODE_AUTO);
  Parameters_FOCUS_MODE_AUTO = NULL;
  if (android_hardware_camera_parameters.FOCUS_MODE_AUTO)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.FOCUS_MODE_AUTO);
  android_hardware_camera_parameters.FOCUS_MODE_AUTO = NULL;
  if (Parameters_FOCUS_MODE_INFINITY)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.FOCUS_MODE_INFINITY,
        Parameters_FOCUS_MODE_INFINITY);
  Parameters_FOCUS_MODE_INFINITY = NULL;
  if (android_hardware_camera_parameters.FOCUS_MODE_INFINITY)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.FOCUS_MODE_INFINITY);
  android_hardware_camera_parameters.FOCUS_MODE_INFINITY = NULL;
  if (Parameters_FOCUS_MODE_MACRO)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.FOCUS_MODE_MACRO,
        Parameters_FOCUS_MODE_MACRO);
  Parameters_FOCUS_MODE_MACRO = NULL;
  if (android_hardware_camera_parameters.FOCUS_MODE_MACRO)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.FOCUS_MODE_MACRO);
  android_hardware_camera_parameters.FOCUS_MODE_MACRO = NULL;
  if (Parameters_FOCUS_MODE_FIXED)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.FOCUS_MODE_FIXED,
        Parameters_FOCUS_MODE_FIXED);
  Parameters_FOCUS_MODE_FIXED = NULL;
  if (android_hardware_camera_parameters.FOCUS_MODE_FIXED)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.FOCUS_MODE_FIXED);
  android_hardware_camera_parameters.FOCUS_MODE_FIXED = NULL;
  if (Parameters_FOCUS_MODE_EDOF)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.FOCUS_MODE_EDOF,
        Parameters_FOCUS_MODE_EDOF);
  Parameters_FOCUS_MODE_EDOF = NULL;
  if (android_hardware_camera_parameters.FOCUS_MODE_EDOF)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.FOCUS_MODE_EDOF);
  android_hardware_camera_parameters.FOCUS_MODE_EDOF = NULL;
  if (Parameters_FOCUS_MODE_CONTINUOUS_VIDEO)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.FOCUS_MODE_CONTINUOUS_VIDEO,
        Parameters_FOCUS_MODE_CONTINUOUS_VIDEO);
  Parameters_FOCUS_MODE_CONTINUOUS_VIDEO = NULL;
  if (android_hardware_camera_parameters.FOCUS_MODE_CONTINUOUS_VIDEO)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.FOCUS_MODE_CONTINUOUS_VIDEO);
  android_hardware_camera_parameters.FOCUS_MODE_CONTINUOUS_VIDEO = NULL;
  if (Parameters_FOCUS_MODE_CONTINUOUS_PICTURE)
    (*env)->ReleaseStringUTFChars (env,
        android_hardware_camera_parameters.FOCUS_MODE_CONTINUOUS_PICTURE,
        Parameters_FOCUS_MODE_CONTINUOUS_PICTURE);
  Parameters_FOCUS_MODE_CONTINUOUS_PICTURE = NULL;
  if (android_hardware_camera_parameters.FOCUS_MODE_CONTINUOUS_PICTURE)
    gst_amc_jni_object_unref (env,
        android_hardware_camera_parameters.FOCUS_MODE_CONTINUOUS_PICTURE);
  android_hardware_camera_parameters.FOCUS_MODE_CONTINUOUS_PICTURE = NULL;

  if (java_lang_string.klass)
    gst_amc_jni_object_unref (env, java_lang_string.klass);
  java_lang_string.klass = NULL;

  if (java_util_list.klass)
    gst_amc_jni_object_unref (env, java_util_list.klass);
  java_util_list.klass = NULL;

  if (java_util_iterator.klass)
    gst_amc_jni_object_unref (env, java_util_iterator.klass);
  java_util_iterator.klass = NULL;

  if (java_lang_integer.klass)
    gst_amc_jni_object_unref (env, java_lang_integer.klass);
  java_lang_integer.klass = NULL;

  if (org_freedesktop_gstreamer_androidmedia_gstahccallback.klass) {
    (*env)->UnregisterNatives (env,
        org_freedesktop_gstreamer_androidmedia_gstahccallback.klass);
    gst_amc_jni_object_unref (env,
        org_freedesktop_gstreamer_androidmedia_gstahccallback.klass);
  }
  org_freedesktop_gstreamer_androidmedia_gstahccallback.klass = NULL;
}

/* android.hardware.Camera */
void
gst_ah_camera_add_callback_buffer (GstAHCamera * self, jbyteArray buffer)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;

  GST_DEBUG ("add callback_buffer %p", buffer);

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera.addCallbackBuffer, buffer);
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.addCallbackBuffer: %s",
        err->message);
    g_clear_error (&err);
  }
}

gboolean
gst_ah_camera_auto_focus (GstAHCamera * self,
    GstAHCAutoFocusCallback cb, gpointer user_data)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jobject object = NULL;
  gboolean ret = FALSE;

  if (cb) {
    object = gst_amc_jni_new_object (env,
        &err,
        FALSE,
        org_freedesktop_gstreamer_androidmedia_gstahccallback.klass,
        org_freedesktop_gstreamer_androidmedia_gstahccallback.constructor,
        *((jlong *) & cb), *((jlong *) & user_data));
    if (err) {
      GST_ERROR
          ("Failed to create org.freedesktop.gstreamer.androidmedia.GstAhcCallback object");
      g_clear_error (&err);
      goto done;
    }
  }

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera.autoFocus, object);
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.autoFocus: %s",
        err->message);
    goto done;
  }

  ret = TRUE;
done:
  if (err)
    g_clear_error (&err);
  if (object)
    gst_amc_jni_object_local_unref (env, object);

  return ret;
}

gboolean
gst_ah_camera_cancel_auto_focus (GstAHCamera * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera.cancelAutoFocus);
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.cancelAutoFocus: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_ah_camera_get_camera_info (gint camera_id, GstAHCCameraInfo * camera_info)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jobject jcamera_info = NULL;
  gboolean ret = FALSE;

  jcamera_info = gst_amc_jni_new_object (env,
      &err,
      FALSE,
      android_hardware_camera_camerainfo.klass,
      android_hardware_camera_camerainfo.constructor);
  if (err) {
    GST_ERROR ("Failed to create android.hardware.camera.CameraInfo object");
    g_clear_error (&err);
    goto done;
  }

  gst_amc_jni_call_static_void_method (env, &err, android_hardware_camera.klass,
      android_hardware_camera.getCameraInfo, camera_id, jcamera_info);
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.getCameraInfo: %s",
        err->message);
    goto done;
  }

  camera_info->facing = (*env)->GetIntField (env, jcamera_info,
      android_hardware_camera_camerainfo.facing);
  if ((*env)->ExceptionCheck (env)) {
    GST_ERROR ("Failed to get CameraInfo.facing field");
    (*env)->ExceptionClear (env);
    goto done;
  }

  camera_info->orientation = (*env)->GetIntField (env, jcamera_info,
      android_hardware_camera_camerainfo.orientation);
  if ((*env)->ExceptionCheck (env)) {
    GST_ERROR ("Failed to get CameraInfo.orientation field");
    (*env)->ExceptionClear (env);
    goto done;
  }

  ret = TRUE;
done:
  if (err)
    g_clear_error (&err);
  if (jcamera_info)
    gst_amc_jni_object_local_unref (env, jcamera_info);

  return ret;
}

gint
gst_ah_camera_get_number_of_cameras (void)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  gint num_cameras;

  gst_amc_jni_call_static_int_method (env, &err, android_hardware_camera.klass,
      android_hardware_camera.getNumberOfCameras, &num_cameras);
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.getNumberOfCameras: %s",
        err->message);
    g_clear_error (&err);
    return -1;
  }

  return num_cameras;
}

GstAHCParameters *
gst_ah_camera_get_parameters (GstAHCamera * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jobject object = NULL;
  GstAHCParameters *params = NULL;

  gst_amc_jni_call_object_method (env, &err, self->object,
      android_hardware_camera.getParameters, &object);
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.getParameters: %s",
        err->message);
    g_clear_error (&err);
    return NULL;
  }
  if (!object) {
    GST_WARNING ("android.hardware.Camera.getParameter is NULL");
    return NULL;
  }

  params = g_slice_new0 (GstAHCParameters);
  params->object = gst_amc_jni_object_ref (env, object);
  gst_amc_jni_object_local_unref (env, object);
  if (!params->object) {
    GST_ERROR ("Failed to create global reference");
    (*env)->ExceptionClear (env);
    g_slice_free (GstAHCParameters, params);
    return NULL;
  }

  GST_DEBUG ("return parameters %p", params->object);

  return params;
}

gboolean
gst_ah_camera_lock (GstAHCamera * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera.lock);
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.lock: %s", err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

GstAHCamera *
gst_ah_camera_open (gint camera_id)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jobject object = NULL;
  GstAHCamera *camera = NULL;

  gst_amc_jni_call_static_object_method (env, &err,
      android_hardware_camera.klass, android_hardware_camera.open, &object,
      camera_id);
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.open: %s", err->message);
    g_clear_error (&err);
    goto done;
  }

  if (object) {
    camera = g_slice_new0 (GstAHCamera);
    camera->object = gst_amc_jni_object_ref (env, object);
    gst_amc_jni_object_local_unref (env, object);
    if (!camera->object) {
      GST_ERROR ("Failed to create global reference");
      (*env)->ExceptionClear (env);
      g_slice_free (GstAHCamera, camera);
      camera = NULL;
    }
  }

done:
  return camera;
}

gboolean
gst_ah_camera_reconnect (GstAHCamera * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera.reconnect);
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.reconnect: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

void
gst_ah_camera_release (GstAHCamera * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera.release);
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.release: %s",
        err->message);
    g_clear_error (&err);
  }
}

void
gst_ah_camera_free (GstAHCamera * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();

  gst_amc_jni_object_unref (env, self->object);
  g_slice_free (GstAHCamera, self);
}


gboolean
gst_ah_camera_set_parameters (GstAHCamera * self, GstAHCParameters * params)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera.setParameters, params->object);
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.setParameters: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_ah_camera_set_error_callback (GstAHCamera * self, GstAHCErrorCallback cb,
    gpointer user_data)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jobject object = NULL;
  gboolean ret = FALSE;

  if (cb) {
    object = gst_amc_jni_new_object (env,
        &err,
        FALSE,
        org_freedesktop_gstreamer_androidmedia_gstahccallback.klass,
        org_freedesktop_gstreamer_androidmedia_gstahccallback.constructor,
        *((jlong *) & cb), *((jlong *) & user_data));
    if (err) {
      GST_ERROR
          ("Failed to create org.freedesktop.gstreamer.androidmedia.GstAhcCallback object");
      g_clear_error (&err);
      goto done;
    }
  }

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera.setErrorCallback, object);
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.setErrorCallback: %s",
        err->message);
    goto done;
  }

  ret = TRUE;
done:
  if (err)
    g_clear_error (&err);
  if (object)
    gst_amc_jni_object_local_unref (env, object);

  return ret;
}

gboolean
gst_ah_camera_set_preview_callback_with_buffer (GstAHCamera * self,
    GstAHCPreviewCallback cb, gpointer user_data)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jobject object = NULL;
  gboolean ret = FALSE;

  if (cb) {
    object = gst_amc_jni_new_object (env,
        &err,
        FALSE,
        org_freedesktop_gstreamer_androidmedia_gstahccallback.klass,
        org_freedesktop_gstreamer_androidmedia_gstahccallback.constructor,
        *((jlong *) & cb), *((jlong *) & user_data));
    if (err) {
      GST_ERROR
          ("Failed to create org.freedesktop.gstreamer.androidmedia.GstAhcCallback object");
      g_clear_error (&err);
      goto done;
    }
  }

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera.setPreviewCallbackWithBuffer, object);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.setPreviewCallbackWithBuffer: %s",
        err->message);
    goto done;
  }

  ret = TRUE;
done:
  if (err)
    g_clear_error (&err);
  if (object)
    gst_amc_jni_object_local_unref (env, object);

  return ret;
}

void
gst_ah_camera_set_preview_texture (GstAHCamera * self,
    GstAmcSurfaceTextureJNI * surfaceTexture)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera.setPreviewTexture,
      gst_amc_surface_texture_jni_get_jobject (surfaceTexture));
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.setPreviewTexture: %s",
        err->message);
    g_clear_error (&err);
  }
}

gboolean
gst_ah_camera_start_preview (GstAHCamera * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera.startPreview);
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.startPreview: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_ah_camera_start_smooth_zoom (GstAHCamera * self, gint value)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera.startSmoothZoom, value);
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.startSmoothZoom: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_ah_camera_stop_preview (GstAHCamera * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera.stopPreview);
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.stopPreview: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_ah_camera_stop_smooth_zoom (GstAHCamera * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera.stopSmoothZoom);
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.stopSmoothZoom: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_ah_camera_unlock (GstAHCamera * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera.unlock);
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.unlock: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

/* android.hardware.Camera.Size */
GstAHCSize *
gst_ahc_size_new (gint width, gint height)
{
  GstAHCSize *self = g_slice_new (GstAHCSize);

  self->width = width;
  self->height = height;

  return self;
}

void
gst_ahc_size_free (GstAHCSize * self)
{
  g_slice_free (GstAHCSize, self);
}

/* java.lang.String */
static jboolean
java_lang_string_equals (JNIEnv * env, jstring str, jstring obj)
{
  return (*env)->CallBooleanMethod (env, str, java_lang_string.equals, obj);
}

/* java.util.List */
static jobject
java_util_list_iterator (JNIEnv * env, jobject obj)
{
  return (*env)->CallObjectMethod (env, obj, java_util_list.iterator);
}

/* java.util.Iterator */
static jobject
java_util_iterator_next (JNIEnv * env, jobject obj)
{
  return (*env)->CallObjectMethod (env, obj, java_util_iterator.next);
}

static jboolean
java_util_iterator_has_next (JNIEnv * env, jobject obj)
{
  return (*env)->CallBooleanMethod (env, obj, java_util_iterator.hasNext);
}

/* java.lang.Integer */
static jint
java_lang_integer_int_value (JNIEnv * env, jobject obj)
{
  return (*env)->CallIntMethod (env, obj, java_lang_integer.intValue);
}


/* android.hardware.Camera.Parameters */
static const gchar *
_white_balance_to_gchar (JNIEnv * env, jstring white_balance)
{
  if (!white_balance)
    return NULL;

  if (java_lang_string_equals (env, white_balance,
          android_hardware_camera_parameters.WHITE_BALANCE_AUTO))
    return Parameters_WHITE_BALANCE_AUTO;
  else if (java_lang_string_equals (env, white_balance,
          android_hardware_camera_parameters.WHITE_BALANCE_INCANDESCENT))
    return Parameters_WHITE_BALANCE_INCANDESCENT;
  else if (java_lang_string_equals (env, white_balance,
          android_hardware_camera_parameters.WHITE_BALANCE_FLUORESCENT))
    return Parameters_WHITE_BALANCE_FLUORESCENT;
  else if (java_lang_string_equals (env, white_balance,
          android_hardware_camera_parameters.WHITE_BALANCE_WARM_FLUORESCENT))
    return Parameters_WHITE_BALANCE_WARM_FLUORESCENT;
  else if (java_lang_string_equals (env, white_balance,
          android_hardware_camera_parameters.WHITE_BALANCE_DAYLIGHT))
    return Parameters_WHITE_BALANCE_DAYLIGHT;
  else if (java_lang_string_equals (env, white_balance,
          android_hardware_camera_parameters.WHITE_BALANCE_CLOUDY_DAYLIGHT))
    return Parameters_WHITE_BALANCE_CLOUDY_DAYLIGHT;
  else if (java_lang_string_equals (env, white_balance,
          android_hardware_camera_parameters.WHITE_BALANCE_TWILIGHT))
    return Parameters_WHITE_BALANCE_TWILIGHT;
  else if (java_lang_string_equals (env, white_balance,
          android_hardware_camera_parameters.WHITE_BALANCE_SHADE))
    return Parameters_WHITE_BALANCE_SHADE;

  return NULL;
}

static jstring
_white_balance_to_jstring (const gchar * white_balance)
{
  if (!white_balance)
    return NULL;

  if (!g_strcmp0 (white_balance, Parameters_WHITE_BALANCE_AUTO))
    return android_hardware_camera_parameters.WHITE_BALANCE_AUTO;
  else if (!g_strcmp0 (white_balance, Parameters_WHITE_BALANCE_INCANDESCENT))
    return android_hardware_camera_parameters.WHITE_BALANCE_INCANDESCENT;
  else if (!g_strcmp0 (white_balance, Parameters_WHITE_BALANCE_FLUORESCENT))
    return android_hardware_camera_parameters.WHITE_BALANCE_FLUORESCENT;
  else if (!g_strcmp0 (white_balance,
          Parameters_WHITE_BALANCE_WARM_FLUORESCENT))
    return android_hardware_camera_parameters.WHITE_BALANCE_WARM_FLUORESCENT;
  else if (!g_strcmp0 (white_balance, Parameters_WHITE_BALANCE_DAYLIGHT))
    return android_hardware_camera_parameters.WHITE_BALANCE_DAYLIGHT;
  else if (!g_strcmp0 (white_balance, Parameters_WHITE_BALANCE_CLOUDY_DAYLIGHT))
    return android_hardware_camera_parameters.WHITE_BALANCE_CLOUDY_DAYLIGHT;
  else if (!g_strcmp0 (white_balance, Parameters_WHITE_BALANCE_TWILIGHT))
    return android_hardware_camera_parameters.WHITE_BALANCE_TWILIGHT;
  else if (!g_strcmp0 (white_balance, Parameters_WHITE_BALANCE_SHADE))
    return android_hardware_camera_parameters.WHITE_BALANCE_SHADE;

  return NULL;
}

static const gchar *
_color_effect_to_gchar (JNIEnv * env, jstring color_effect)
{
  if (!color_effect)
    return NULL;

  if (java_lang_string_equals (env, color_effect,
          android_hardware_camera_parameters.EFFECT_NONE))
    return Parameters_EFFECT_NONE;
  else if (java_lang_string_equals (env, color_effect,
          android_hardware_camera_parameters.EFFECT_MONO))
    return Parameters_EFFECT_MONO;
  else if (java_lang_string_equals (env, color_effect,
          android_hardware_camera_parameters.EFFECT_NEGATIVE))
    return Parameters_EFFECT_NEGATIVE;
  else if (java_lang_string_equals (env, color_effect,
          android_hardware_camera_parameters.EFFECT_SOLARIZE))
    return Parameters_EFFECT_SOLARIZE;
  else if (java_lang_string_equals (env, color_effect,
          android_hardware_camera_parameters.EFFECT_SEPIA))
    return Parameters_EFFECT_SEPIA;
  else if (java_lang_string_equals (env, color_effect,
          android_hardware_camera_parameters.EFFECT_POSTERIZE))
    return Parameters_EFFECT_POSTERIZE;
  else if (java_lang_string_equals (env, color_effect,
          android_hardware_camera_parameters.EFFECT_WHITEBOARD))
    return Parameters_EFFECT_WHITEBOARD;
  else if (java_lang_string_equals (env, color_effect,
          android_hardware_camera_parameters.EFFECT_BLACKBOARD))
    return Parameters_EFFECT_BLACKBOARD;
  else if (java_lang_string_equals (env, color_effect,
          android_hardware_camera_parameters.EFFECT_AQUA))
    return Parameters_EFFECT_AQUA;
  else if (android_hardware_camera_parameters.EFFECT_EMBOSS != NULL &&
      java_lang_string_equals (env, color_effect,
          android_hardware_camera_parameters.EFFECT_EMBOSS))
    return Parameters_EFFECT_EMBOSS;
  else if (android_hardware_camera_parameters.EFFECT_SKETCH != NULL &&
      java_lang_string_equals (env, color_effect,
          android_hardware_camera_parameters.EFFECT_SKETCH))
    return Parameters_EFFECT_SKETCH;
  else if (android_hardware_camera_parameters.EFFECT_NEON != NULL &&
      java_lang_string_equals (env, color_effect,
          android_hardware_camera_parameters.EFFECT_NEON))
    return Parameters_EFFECT_NEON;

  return NULL;
}

static jstring
_color_effect_to_jstring (const gchar * color_effect)
{
  if (!color_effect)
    return NULL;

  if (!g_strcmp0 (color_effect, Parameters_EFFECT_NONE))
    return android_hardware_camera_parameters.EFFECT_NONE;
  else if (!g_strcmp0 (color_effect, Parameters_EFFECT_MONO))
    return android_hardware_camera_parameters.EFFECT_MONO;
  else if (!g_strcmp0 (color_effect, Parameters_EFFECT_NEGATIVE))
    return android_hardware_camera_parameters.EFFECT_NEGATIVE;
  else if (!g_strcmp0 (color_effect, Parameters_EFFECT_SOLARIZE))
    return android_hardware_camera_parameters.EFFECT_SOLARIZE;
  else if (!g_strcmp0 (color_effect, Parameters_EFFECT_SEPIA))
    return android_hardware_camera_parameters.EFFECT_SEPIA;
  else if (!g_strcmp0 (color_effect, Parameters_EFFECT_POSTERIZE))
    return android_hardware_camera_parameters.EFFECT_POSTERIZE;
  else if (!g_strcmp0 (color_effect, Parameters_EFFECT_WHITEBOARD))
    return android_hardware_camera_parameters.EFFECT_WHITEBOARD;
  else if (!g_strcmp0 (color_effect, Parameters_EFFECT_BLACKBOARD))
    return android_hardware_camera_parameters.EFFECT_BLACKBOARD;
  else if (!g_strcmp0 (color_effect, Parameters_EFFECT_AQUA))
    return android_hardware_camera_parameters.EFFECT_AQUA;
  else if (android_hardware_camera_parameters.EFFECT_EMBOSS != NULL
      && !g_strcmp0 (color_effect, Parameters_EFFECT_EMBOSS))
    return android_hardware_camera_parameters.EFFECT_EMBOSS;
  else if (android_hardware_camera_parameters.EFFECT_SKETCH != NULL
      && !g_strcmp0 (color_effect, Parameters_EFFECT_SKETCH))
    return android_hardware_camera_parameters.EFFECT_SKETCH;
  else if (android_hardware_camera_parameters.EFFECT_NEON != NULL
      && !g_strcmp0 (color_effect, Parameters_EFFECT_NEON))
    return android_hardware_camera_parameters.EFFECT_NEON;

  return NULL;
}

static const gchar *
_antibanding_to_gchar (JNIEnv * env, jstring antibanding)
{
  if (!antibanding)
    return NULL;

  if (java_lang_string_equals (env, antibanding,
          android_hardware_camera_parameters.ANTIBANDING_AUTO))
    return Parameters_ANTIBANDING_AUTO;
  else if (java_lang_string_equals (env, antibanding,
          android_hardware_camera_parameters.ANTIBANDING_50HZ))
    return Parameters_ANTIBANDING_50HZ;
  else if (java_lang_string_equals (env, antibanding,
          android_hardware_camera_parameters.ANTIBANDING_60HZ))
    return Parameters_ANTIBANDING_60HZ;
  else if (java_lang_string_equals (env, antibanding,
          android_hardware_camera_parameters.ANTIBANDING_OFF))
    return Parameters_ANTIBANDING_OFF;

  return NULL;
}

static jstring
_antibanding_to_jstring (const gchar * antibanding)
{
  if (!antibanding)
    return NULL;

  if (!g_strcmp0 (antibanding, Parameters_ANTIBANDING_AUTO))
    return android_hardware_camera_parameters.ANTIBANDING_AUTO;
  else if (!g_strcmp0 (antibanding, Parameters_ANTIBANDING_50HZ))
    return android_hardware_camera_parameters.ANTIBANDING_50HZ;
  else if (!g_strcmp0 (antibanding, Parameters_ANTIBANDING_60HZ))
    return android_hardware_camera_parameters.ANTIBANDING_60HZ;
  else if (!g_strcmp0 (antibanding, Parameters_ANTIBANDING_OFF))
    return android_hardware_camera_parameters.ANTIBANDING_OFF;

  return NULL;
}

static const gchar *
_flash_mode_to_gchar (JNIEnv * env, jstring flash_mode)
{
  if (!flash_mode)
    return NULL;

  if (java_lang_string_equals (env, flash_mode,
          android_hardware_camera_parameters.FLASH_MODE_OFF))
    return Parameters_FLASH_MODE_OFF;
  else if (java_lang_string_equals (env, flash_mode,
          android_hardware_camera_parameters.FLASH_MODE_AUTO))
    return Parameters_FLASH_MODE_AUTO;
  else if (java_lang_string_equals (env, flash_mode,
          android_hardware_camera_parameters.FLASH_MODE_ON))
    return Parameters_FLASH_MODE_ON;
  else if (java_lang_string_equals (env, flash_mode,
          android_hardware_camera_parameters.FLASH_MODE_RED_EYE))
    return Parameters_FLASH_MODE_RED_EYE;
  else if (java_lang_string_equals (env, flash_mode,
          android_hardware_camera_parameters.FLASH_MODE_TORCH))
    return Parameters_FLASH_MODE_TORCH;

  return NULL;
}

static jstring
_flash_mode_to_jstring (const gchar * flash_mode)
{
  if (!flash_mode)
    return NULL;

  if (!g_strcmp0 (flash_mode, Parameters_FLASH_MODE_OFF))
    return android_hardware_camera_parameters.FLASH_MODE_OFF;
  else if (!g_strcmp0 (flash_mode, Parameters_FLASH_MODE_AUTO))
    return android_hardware_camera_parameters.FLASH_MODE_AUTO;
  else if (!g_strcmp0 (flash_mode, Parameters_FLASH_MODE_ON))
    return android_hardware_camera_parameters.FLASH_MODE_ON;
  else if (!g_strcmp0 (flash_mode, Parameters_FLASH_MODE_RED_EYE))
    return android_hardware_camera_parameters.FLASH_MODE_RED_EYE;
  else if (!g_strcmp0 (flash_mode, Parameters_FLASH_MODE_TORCH))
    return android_hardware_camera_parameters.FLASH_MODE_TORCH;

  return NULL;
}

static const gchar *
_scene_mode_to_gchar (JNIEnv * env, jstring scene_mode)
{
  if (!scene_mode)
    return NULL;

  if (java_lang_string_equals (env, scene_mode,
          android_hardware_camera_parameters.SCENE_MODE_AUTO))
    return Parameters_SCENE_MODE_AUTO;
  else if (java_lang_string_equals (env, scene_mode,
          android_hardware_camera_parameters.SCENE_MODE_ACTION))
    return Parameters_SCENE_MODE_ACTION;
  else if (java_lang_string_equals (env, scene_mode,
          android_hardware_camera_parameters.SCENE_MODE_PORTRAIT))
    return Parameters_SCENE_MODE_PORTRAIT;
  else if (java_lang_string_equals (env, scene_mode,
          android_hardware_camera_parameters.SCENE_MODE_LANDSCAPE))
    return Parameters_SCENE_MODE_LANDSCAPE;
  else if (java_lang_string_equals (env, scene_mode,
          android_hardware_camera_parameters.SCENE_MODE_NIGHT))
    return Parameters_SCENE_MODE_NIGHT;
  else if (java_lang_string_equals (env, scene_mode,
          android_hardware_camera_parameters.SCENE_MODE_NIGHT_PORTRAIT))
    return Parameters_SCENE_MODE_NIGHT_PORTRAIT;
  else if (java_lang_string_equals (env, scene_mode,
          android_hardware_camera_parameters.SCENE_MODE_THEATRE))
    return Parameters_SCENE_MODE_THEATRE;
  else if (java_lang_string_equals (env, scene_mode,
          android_hardware_camera_parameters.SCENE_MODE_BEACH))
    return Parameters_SCENE_MODE_BEACH;
  else if (java_lang_string_equals (env, scene_mode,
          android_hardware_camera_parameters.SCENE_MODE_SNOW))
    return Parameters_SCENE_MODE_SNOW;
  else if (java_lang_string_equals (env, scene_mode,
          android_hardware_camera_parameters.SCENE_MODE_SUNSET))
    return Parameters_SCENE_MODE_SUNSET;
  else if (java_lang_string_equals (env, scene_mode,
          android_hardware_camera_parameters.SCENE_MODE_STEADYPHOTO))
    return Parameters_SCENE_MODE_STEADYPHOTO;
  else if (java_lang_string_equals (env, scene_mode,
          android_hardware_camera_parameters.SCENE_MODE_FIREWORKS))
    return Parameters_SCENE_MODE_FIREWORKS;
  else if (java_lang_string_equals (env, scene_mode,
          android_hardware_camera_parameters.SCENE_MODE_SPORTS))
    return Parameters_SCENE_MODE_SPORTS;
  else if (java_lang_string_equals (env, scene_mode,
          android_hardware_camera_parameters.SCENE_MODE_PARTY))
    return Parameters_SCENE_MODE_PARTY;
  else if (java_lang_string_equals (env, scene_mode,
          android_hardware_camera_parameters.SCENE_MODE_CANDLELIGHT))
    return Parameters_SCENE_MODE_CANDLELIGHT;
  else if (java_lang_string_equals (env, scene_mode,
          android_hardware_camera_parameters.SCENE_MODE_BARCODE))
    return Parameters_SCENE_MODE_BARCODE;
  else if (android_hardware_camera_parameters.SCENE_MODE_BACKLIGHT != NULL &&
      java_lang_string_equals (env, scene_mode,
          android_hardware_camera_parameters.SCENE_MODE_BACKLIGHT))
    return Parameters_SCENE_MODE_BACKLIGHT;
  else if (android_hardware_camera_parameters.SCENE_MODE_FLOWERS != NULL &&
      java_lang_string_equals (env, scene_mode,
          android_hardware_camera_parameters.SCENE_MODE_FLOWERS))
    return Parameters_SCENE_MODE_FLOWERS;
  else if (android_hardware_camera_parameters.SCENE_MODE_AR != NULL &&
      java_lang_string_equals (env, scene_mode,
          android_hardware_camera_parameters.SCENE_MODE_AR))
    return Parameters_SCENE_MODE_AR;
  else if (android_hardware_camera_parameters.SCENE_MODE_HDR != NULL &&
      java_lang_string_equals (env, scene_mode,
          android_hardware_camera_parameters.SCENE_MODE_HDR))
    return Parameters_SCENE_MODE_HDR;

  return NULL;
}

static const jstring
_scene_mode_to_jstring (const gchar * scene_mode)
{
  if (!scene_mode)
    return NULL;

  if (!g_strcmp0 (scene_mode, Parameters_SCENE_MODE_AUTO))
    return android_hardware_camera_parameters.SCENE_MODE_AUTO;
  else if (!g_strcmp0 (scene_mode, Parameters_SCENE_MODE_ACTION))
    return android_hardware_camera_parameters.SCENE_MODE_ACTION;
  else if (!g_strcmp0 (scene_mode, Parameters_SCENE_MODE_PORTRAIT))
    return android_hardware_camera_parameters.SCENE_MODE_PORTRAIT;
  else if (!g_strcmp0 (scene_mode, Parameters_SCENE_MODE_LANDSCAPE))
    return android_hardware_camera_parameters.SCENE_MODE_LANDSCAPE;
  else if (!g_strcmp0 (scene_mode, Parameters_SCENE_MODE_NIGHT))
    return android_hardware_camera_parameters.SCENE_MODE_NIGHT;
  else if (!g_strcmp0 (scene_mode, Parameters_SCENE_MODE_NIGHT_PORTRAIT))
    return android_hardware_camera_parameters.SCENE_MODE_NIGHT_PORTRAIT;
  else if (!g_strcmp0 (scene_mode, Parameters_SCENE_MODE_THEATRE))
    return android_hardware_camera_parameters.SCENE_MODE_THEATRE;
  else if (!g_strcmp0 (scene_mode, Parameters_SCENE_MODE_BEACH))
    return android_hardware_camera_parameters.SCENE_MODE_BEACH;
  else if (!g_strcmp0 (scene_mode, Parameters_SCENE_MODE_SNOW))
    return android_hardware_camera_parameters.SCENE_MODE_SNOW;
  else if (!g_strcmp0 (scene_mode, Parameters_SCENE_MODE_SUNSET))
    return android_hardware_camera_parameters.SCENE_MODE_SUNSET;
  else if (!g_strcmp0 (scene_mode, Parameters_SCENE_MODE_STEADYPHOTO))
    return android_hardware_camera_parameters.SCENE_MODE_STEADYPHOTO;
  else if (!g_strcmp0 (scene_mode, Parameters_SCENE_MODE_FIREWORKS))
    return android_hardware_camera_parameters.SCENE_MODE_FIREWORKS;
  else if (!g_strcmp0 (scene_mode, Parameters_SCENE_MODE_SPORTS))
    return android_hardware_camera_parameters.SCENE_MODE_SPORTS;
  else if (!g_strcmp0 (scene_mode, Parameters_SCENE_MODE_PARTY))
    return android_hardware_camera_parameters.SCENE_MODE_PARTY;
  else if (!g_strcmp0 (scene_mode, Parameters_SCENE_MODE_CANDLELIGHT))
    return android_hardware_camera_parameters.SCENE_MODE_CANDLELIGHT;
  else if (!g_strcmp0 (scene_mode, Parameters_SCENE_MODE_BARCODE))
    return android_hardware_camera_parameters.SCENE_MODE_BARCODE;
  else if (android_hardware_camera_parameters.SCENE_MODE_BACKLIGHT != NULL
      && !g_strcmp0 (scene_mode, Parameters_SCENE_MODE_BACKLIGHT))
    return android_hardware_camera_parameters.SCENE_MODE_BACKLIGHT;
  else if (android_hardware_camera_parameters.SCENE_MODE_FLOWERS != NULL
      && !g_strcmp0 (scene_mode, Parameters_SCENE_MODE_FLOWERS))
    return android_hardware_camera_parameters.SCENE_MODE_FLOWERS;
  else if (android_hardware_camera_parameters.SCENE_MODE_AR != NULL
      && !g_strcmp0 (scene_mode, Parameters_SCENE_MODE_AR))
    return android_hardware_camera_parameters.SCENE_MODE_AR;
  else if (android_hardware_camera_parameters.SCENE_MODE_HDR != NULL
      && !g_strcmp0 (scene_mode, Parameters_SCENE_MODE_HDR))
    return android_hardware_camera_parameters.SCENE_MODE_HDR;

  return NULL;
}

static const gchar *
_focus_mode_to_gchar (JNIEnv * env, jstring focus_mode)
{
  if (!focus_mode)
    return NULL;

  if (java_lang_string_equals (env, focus_mode,
          android_hardware_camera_parameters.FOCUS_MODE_AUTO))
    return Parameters_FOCUS_MODE_AUTO;
  else if (java_lang_string_equals (env, focus_mode,
          android_hardware_camera_parameters.FOCUS_MODE_INFINITY))
    return Parameters_FOCUS_MODE_INFINITY;
  else if (java_lang_string_equals (env, focus_mode,
          android_hardware_camera_parameters.FOCUS_MODE_MACRO))
    return Parameters_FOCUS_MODE_MACRO;
  else if (java_lang_string_equals (env, focus_mode,
          android_hardware_camera_parameters.FOCUS_MODE_FIXED))
    return Parameters_FOCUS_MODE_FIXED;
  else if (java_lang_string_equals (env, focus_mode,
          android_hardware_camera_parameters.FOCUS_MODE_EDOF))
    return Parameters_FOCUS_MODE_EDOF;
  else if (java_lang_string_equals (env, focus_mode,
          android_hardware_camera_parameters.FOCUS_MODE_CONTINUOUS_VIDEO))
    return Parameters_FOCUS_MODE_CONTINUOUS_VIDEO;
  else if (java_lang_string_equals (env, focus_mode,
          android_hardware_camera_parameters.FOCUS_MODE_CONTINUOUS_PICTURE))
    return Parameters_FOCUS_MODE_CONTINUOUS_PICTURE;

  return NULL;
}

static jstring
_focus_mode_to_jstring (const gchar * focus_mode)
{
  if (!focus_mode)
    return NULL;

  if (!g_strcmp0 (focus_mode, Parameters_FOCUS_MODE_AUTO))
    return android_hardware_camera_parameters.FOCUS_MODE_AUTO;
  else if (!g_strcmp0 (focus_mode, Parameters_FOCUS_MODE_INFINITY))
    return android_hardware_camera_parameters.FOCUS_MODE_INFINITY;
  else if (!g_strcmp0 (focus_mode, Parameters_FOCUS_MODE_MACRO))
    return android_hardware_camera_parameters.FOCUS_MODE_MACRO;
  else if (!g_strcmp0 (focus_mode, Parameters_FOCUS_MODE_FIXED))
    return android_hardware_camera_parameters.FOCUS_MODE_FIXED;
  else if (!g_strcmp0 (focus_mode, Parameters_FOCUS_MODE_EDOF))
    return android_hardware_camera_parameters.FOCUS_MODE_EDOF;
  else if (!g_strcmp0 (focus_mode, Parameters_FOCUS_MODE_CONTINUOUS_VIDEO))
    return android_hardware_camera_parameters.FOCUS_MODE_CONTINUOUS_VIDEO;
  else if (!g_strcmp0 (focus_mode, Parameters_FOCUS_MODE_CONTINUOUS_PICTURE))
    return android_hardware_camera_parameters.FOCUS_MODE_CONTINUOUS_PICTURE;

  return NULL;
}

gchar *
gst_ahc_parameters_flatten (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jstring v_str = NULL;
  const gchar *v = NULL;
  gchar *ret = NULL;

  gst_amc_jni_call_object_method (env, &err, self->object,
      android_hardware_camera_parameters.flatten, &v_str);
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.Parameters.flatten: %s",
        err->message);
    goto done;
  }

  v = (*env)->GetStringUTFChars (env, v_str, NULL);
  if (!v) {
    GST_ERROR ("Failed to convert string to UTF8");
    (*env)->ExceptionClear (env);
    goto done;
  }

  ret = g_strdup (v);
done:
  if (err)
    g_clear_error (&err);
  if (v)
    (*env)->ReleaseStringUTFChars (env, v_str, v);
  if (v_str)
    gst_amc_jni_object_local_unref (env, v_str);

  return ret;
}

const gchar *
gst_ahc_parameters_get_antibanding (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  const gchar *ret = NULL;
  jstring antibanding;

  gst_amc_jni_call_object_method (env, &err, self->object,
      android_hardware_camera_parameters.getAntibanding, &antibanding);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getAntibanding: %s",
        err->message);
    g_clear_error (&err);
    return NULL;
  }

  ret = _antibanding_to_gchar (env, antibanding);

  if (antibanding)
    gst_amc_jni_object_local_unref (env, antibanding);

  return ret;
}

const gchar *
gst_ahc_parameters_get_color_effect (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  const gchar *ret = NULL;
  jstring color_effect;

  gst_amc_jni_call_object_method (env, &err, self->object,
      android_hardware_camera_parameters.getColorEffect, &color_effect);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getColorEffect: %s",
        err->message);
    g_clear_error (&err);
    return NULL;
  }

  ret = _color_effect_to_gchar (env, color_effect);

  if (color_effect)
    gst_amc_jni_object_local_unref (env, color_effect);

  return ret;
}

gint
gst_ahc_parameters_get_exposure_compensation (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  gint ev;

  gst_amc_jni_call_int_method (env, &err, self->object,
      android_hardware_camera_parameters.getExposureCompensation, &ev);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getExposureCompensation: %s",
        err->message);
    g_clear_error (&err);
    return -1;
  }

  return ev;
}

gfloat
gst_ahc_parameters_get_exposure_compensation_step (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  gfloat step;

  gst_amc_jni_call_float_method (env, &err, self->object,
      android_hardware_camera_parameters.getExposureCompensationStep, &step);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getExposureCompensationStep: %s",
        err->message);
    g_clear_error (&err);
    return 0.0;
  }

  return step;
}

const gchar *
gst_ahc_parameters_get_flash_mode (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  const gchar *ret = NULL;
  jstring flash_mode;

  gst_amc_jni_call_object_method (env, &err, self->object,
      android_hardware_camera_parameters.getFlashMode, &flash_mode);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getFlashMode: %s",
        err->message);
    g_clear_error (&err);
    return NULL;
  }

  ret = _flash_mode_to_gchar (env, flash_mode);

  if (flash_mode)
    gst_amc_jni_object_local_unref (env, flash_mode);

  return ret;
}

gfloat
gst_ahc_parameters_get_focal_length (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  gfloat length;

  gst_amc_jni_call_float_method (env, &err, self->object,
      android_hardware_camera_parameters.getFocalLength, &length);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getFocalLength: %s",
        err->message);
    g_clear_error (&err);
    return 0.0;
  }

  return length;
}

const gchar *
gst_ahc_parameters_get_focus_mode (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  const gchar *ret = NULL;
  jstring focus_mode;

  gst_amc_jni_call_object_method (env, &err, self->object,
      android_hardware_camera_parameters.getFocusMode, &focus_mode);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getFocusMode: %s",
        err->message);
    g_clear_error (&err);
    return NULL;
  }

  ret = _focus_mode_to_gchar (env, focus_mode);

  if (focus_mode)
    gst_amc_jni_object_local_unref (env, focus_mode);

  return ret;
}

gfloat
gst_ahc_parameters_get_horizontal_view_angle (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  gfloat angle;

  gst_amc_jni_call_float_method (env, &err, self->object,
      android_hardware_camera_parameters.getHorizontalViewAngle, &angle);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getHorizontalViewAngle: %s",
        err->message);
    g_clear_error (&err);
    return 0.0;
  }

  return angle;
}

gint
gst_ahc_parameters_get_max_exposure_compensation (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  gint max;

  gst_amc_jni_call_int_method (env, &err, self->object,
      android_hardware_camera_parameters.getMaxExposureCompensation, &max);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getMaxExposureCompensation: %s",
        err->message);
    g_clear_error (&err);
    return 0;
  }

  return max;
}

gint
gst_ahc_parameters_get_max_zoom (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  gint max;

  gst_amc_jni_call_int_method (env, &err, self->object,
      android_hardware_camera_parameters.getMaxZoom, &max);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getMaxZoom: %s",
        err->message);
    g_clear_error (&err);
    return -1;
  }

  return max;
}

gint
gst_ahc_parameters_get_min_exposure_compensation (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  gint min;

  gst_amc_jni_call_int_method (env, &err, self->object,
      android_hardware_camera_parameters.getMinExposureCompensation, &min);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getMinExposureCompensation: %s",
        err->message);
    g_clear_error (&err);
    return 0;
  }

  return min;
}

gint
gst_ahc_parameters_get_preview_format (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  gint format;

  gst_amc_jni_call_int_method (env, &err, self->object,
      android_hardware_camera_parameters.getPreviewFormat, &format);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getPreviewFormat: %s",
        err->message);
    g_clear_error (&err);
    return 0;
  }

  return format;
}

gboolean
gst_ahc_parameters_get_preview_fps_range (GstAHCParameters * self,
    gint * min, gint * max)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  gboolean ret = FALSE;
  jintArray range = NULL;
  jint *fps = NULL;

  range = (*env)->NewIntArray (env, 2);
  if (!fps) {
    (*env)->ExceptionClear (env);
    GST_ERROR ("Failed to create array");
    goto done;
  }

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera_parameters.getPreviewFpsRange, range);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getPreviewFpsRange: %s",
        err->message);
    goto done;
  }

  fps = (*env)->GetIntArrayElements (env, range, NULL);
  if ((*env)->ExceptionCheck (env) || !fps) {
    (*env)->ExceptionClear (env);
    GST_ERROR ("Failed to get array elements");
    goto done;
  }
  if (min)
    *min = fps[0];
  if (max)
    *max = fps[1];

  ret = TRUE;
done:
  if (err)
    g_clear_error (&err);
  if (fps)
    (*env)->ReleaseIntArrayElements (env, range, fps, JNI_ABORT);
  if (range)
    gst_amc_jni_object_local_unref (env, range);

  return ret;
}

GstAHCSize *
gst_ahc_parameters_get_preview_size (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jobject jsize = NULL;
  GstAHCSize *size = NULL;

  gst_amc_jni_call_object_method (env, &err, self->object,
      android_hardware_camera_parameters.getPreviewSize, &jsize);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getPreviewSize: %s",
        err->message);
    goto done;
  }

  size = g_slice_new (GstAHCSize);
  size->width = (*env)->GetIntField (env, jsize,
      android_hardware_camera_size.width);
  if ((*env)->ExceptionCheck (env)) {
    GST_ERROR ("Failed to get Camera.Size.width field");
    (*env)->ExceptionClear (env);
    g_slice_free (GstAHCSize, size);
    size = NULL;
    goto done;
  }

  size->height = (*env)->GetIntField (env, jsize,
      android_hardware_camera_size.height);
  if ((*env)->ExceptionCheck (env)) {
    GST_ERROR ("Failed to get Camera.Size.height field");
    (*env)->ExceptionClear (env);
    g_slice_free (GstAHCSize, size);
    size = NULL;
    goto done;
  }

done:
  if (err)
    g_clear_error (&err);
  if (jsize)
    gst_amc_jni_object_local_unref (env, jsize);

  return size;
}

const gchar *
gst_ahc_parameters_get_scene_mode (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  const gchar *ret = NULL;
  jstring scene_mode;

  gst_amc_jni_call_object_method (env, &err, self->object,
      android_hardware_camera_parameters.getSceneMode, &scene_mode);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getSceneMode: %s",
        err->message);
    g_clear_error (&err);
    return NULL;
  }

  ret = _scene_mode_to_gchar (env, scene_mode);

  if (scene_mode)
    gst_amc_jni_object_local_unref (env, scene_mode);

  return ret;
}

GList *
gst_ahc_parameters_get_supported_antibanding (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jobject list = NULL;
  GList *ret = NULL;

  gst_amc_jni_call_object_method (env, &err, self->object,
      android_hardware_camera_parameters.getSupportedAntibanding, &list);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getSupportedAntibanding: %s",
        err->message);
    g_clear_error (&err);
    return NULL;
  }

  if (list) {
    jobject iterator = NULL;

    iterator = java_util_list_iterator (env, list);
    if (iterator) {
      while (java_util_iterator_has_next (env, iterator)) {
        jobject str = java_util_iterator_next (env, iterator);

        if (str) {
          const gchar *value = _antibanding_to_gchar (env, str);

          ret = g_list_append (ret, (gchar *) value);
          gst_amc_jni_object_local_unref (env, str);
        }
      }
      gst_amc_jni_object_local_unref (env, iterator);
    }
    gst_amc_jni_object_local_unref (env, list);
  }

  return ret;
}

void
gst_ahc_parameters_supported_antibanding_free (GList * list)
{
  g_list_free (list);
}

GList *
gst_ahc_parameters_get_supported_color_effects (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jobject list = NULL;
  GList *ret = NULL;

  gst_amc_jni_call_object_method (env, &err, self->object,
      android_hardware_camera_parameters.getSupportedColorEffects, &list);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getSupportedColorEffects: %s",
        err->message);
    g_clear_error (&err);
    return NULL;
  }

  if (list) {
    jobject iterator = NULL;

    iterator = java_util_list_iterator (env, list);
    if (iterator) {
      while (java_util_iterator_has_next (env, iterator)) {
        jobject str = java_util_iterator_next (env, iterator);

        if (str) {
          const gchar *value = _color_effect_to_gchar (env, str);

          ret = g_list_append (ret, (gchar *) value);
          gst_amc_jni_object_local_unref (env, str);
        }
      }
      gst_amc_jni_object_local_unref (env, iterator);
    }
    gst_amc_jni_object_local_unref (env, list);
  }

  return ret;
}

void
gst_ahc_parameters_supported_color_effects_free (GList * list)
{
  g_list_free (list);
}

GList *
gst_ahc_parameters_get_supported_flash_modes (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jobject list = NULL;
  GList *ret = NULL;

  gst_amc_jni_call_object_method (env, &err, self->object,
      android_hardware_camera_parameters.getSupportedFlashModes, &list);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getSupportedFlashModes: %s",
        err->message);
    g_clear_error (&err);
    return NULL;
  }

  if (list) {
    jobject iterator = NULL;

    iterator = java_util_list_iterator (env, list);
    if (iterator) {
      while (java_util_iterator_has_next (env, iterator)) {
        jobject str = java_util_iterator_next (env, iterator);

        if (str) {
          const gchar *value = _flash_mode_to_gchar (env, str);

          ret = g_list_append (ret, (gchar *) value);
          gst_amc_jni_object_local_unref (env, str);
        }
      }
      gst_amc_jni_object_local_unref (env, iterator);
    }
    gst_amc_jni_object_local_unref (env, list);
  }

  return ret;
}

void
gst_ahc_parameters_supported_flash_modes_free (GList * list)
{
  g_list_free (list);
}

GList *
gst_ahc_parameters_get_supported_focus_modes (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jobject list = NULL;
  GList *ret = NULL;

  gst_amc_jni_call_object_method (env, &err, self->object,
      android_hardware_camera_parameters.getSupportedFocusModes, &list);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getSupportedFocusModes: %s",
        err->message);
    g_clear_error (&err);
    return NULL;
  }

  if (list) {
    jobject iterator = NULL;

    iterator = java_util_list_iterator (env, list);
    if (iterator) {
      while (java_util_iterator_has_next (env, iterator)) {
        jobject str = java_util_iterator_next (env, iterator);

        if (str) {
          const gchar *value = _focus_mode_to_gchar (env, str);

          ret = g_list_append (ret, (gchar *) value);
          gst_amc_jni_object_local_unref (env, str);
        }
      }
      gst_amc_jni_object_local_unref (env, iterator);
    }
    gst_amc_jni_object_local_unref (env, list);
  }

  return ret;
}

void
gst_ahc_parameters_supported_focus_modes_free (GList * list)
{
  g_list_free (list);
}

GList *
gst_ahc_parameters_get_supported_preview_formats (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jobject list = NULL;
  GList *ret = NULL;

  gst_amc_jni_call_object_method (env, &err, self->object,
      android_hardware_camera_parameters.getSupportedPreviewFormats, &list);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getSupportedPreviewFormats: %s",
        err->message);
    g_clear_error (&err);
    return NULL;
  }

  if (list) {
    jobject iterator = NULL;

    iterator = java_util_list_iterator (env, list);
    if (iterator) {
      while (java_util_iterator_has_next (env, iterator)) {
        jobject integer = java_util_iterator_next (env, iterator);

        if (integer) {
          jint value = java_lang_integer_int_value (env, integer);

          ret = g_list_append (ret, GINT_TO_POINTER (value));
          gst_amc_jni_object_local_unref (env, integer);
        }
      }
      gst_amc_jni_object_local_unref (env, iterator);
    }
    gst_amc_jni_object_local_unref (env, list);
  }

  return ret;
}

void
gst_ahc_parameters_supported_preview_formats_free (GList * list)
{
  g_list_free (list);
}

GList *
gst_ahc_parameters_get_supported_preview_fps_range (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jobject list = NULL;
  GList *ret = NULL;

  gst_amc_jni_call_object_method (env, &err, self->object,
      android_hardware_camera_parameters.getSupportedPreviewFpsRange, &list);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getSupportedPreviewFpsRange: %s",
        err->message);
    g_clear_error (&err);
    return NULL;
  }

  if (list) {
    jobject iterator = NULL;

    iterator = java_util_list_iterator (env, list);
    if (iterator) {
      while (java_util_iterator_has_next (env, iterator)) {
        jintArray range = java_util_iterator_next (env, iterator);

        if (range) {
          jint *fps = g_new (jint, 2);

          (*env)->GetIntArrayRegion (env, range, 0, 2, fps);
          ret = g_list_append (ret, fps);
          gst_amc_jni_object_local_unref (env, range);
        }
      }
      gst_amc_jni_object_local_unref (env, iterator);
    }
    gst_amc_jni_object_local_unref (env, list);
  }

  return ret;
}

void
gst_ahc_parameters_supported_preview_fps_range_free (GList * list)
{
  g_list_foreach (list, (GFunc) g_free, NULL);
  g_list_free (list);
}

GList *
gst_ahc_parameters_get_supported_preview_sizes (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jobject list = NULL;
  GList *ret = NULL;

  gst_amc_jni_call_object_method (env, &err, self->object,
      android_hardware_camera_parameters.getSupportedPreviewSizes, &list);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getSupportedPreviewSizes: %s",
        err->message);
    g_clear_error (&err);
    return NULL;
  }

  if (list) {
    jobject iterator = NULL;

    iterator = java_util_list_iterator (env, list);
    if (iterator) {
      while (java_util_iterator_has_next (env, iterator)) {
        jobject jsize = java_util_iterator_next (env, iterator);

        if (jsize) {
          jint width, height;

          width = (*env)->GetIntField (env, jsize,
              android_hardware_camera_size.width);
          height = (*env)->GetIntField (env, jsize,
              android_hardware_camera_size.height);

          ret = g_list_append (ret, gst_ahc_size_new (width, height));
          gst_amc_jni_object_local_unref (env, jsize);
        }
      }
      gst_amc_jni_object_local_unref (env, iterator);
    }
    gst_amc_jni_object_local_unref (env, list);
  }

  return ret;
}

void
gst_ahc_parameters_supported_preview_sizes_free (GList * list)
{
  g_list_foreach (list, (GFunc) gst_ahc_size_free, NULL);
  g_list_free (list);
}

GList *
gst_ahc_parameters_get_supported_scene_modes (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jobject list = NULL;
  GList *ret = NULL;

  gst_amc_jni_call_object_method (env, &err, self->object,
      android_hardware_camera_parameters.getSupportedSceneModes, &list);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getSupportedSceneModes: %s",
        err->message);
    g_clear_error (&err);
    return NULL;
  }

  if (list) {
    jobject iterator = NULL;

    iterator = java_util_list_iterator (env, list);
    if (iterator) {
      while (java_util_iterator_has_next (env, iterator)) {
        jobject str = java_util_iterator_next (env, iterator);

        if (str) {
          const gchar *value = _scene_mode_to_gchar (env, str);

          ret = g_list_append (ret, (gchar *) value);
          gst_amc_jni_object_local_unref (env, str);
        }
      }
      gst_amc_jni_object_local_unref (env, iterator);
    }
    gst_amc_jni_object_local_unref (env, list);
  }

  return ret;
}

void
gst_ahc_parameters_supported_scene_modes_free (GList * list)
{
  g_list_free (list);
}

GList *
gst_ahc_parameters_get_supported_white_balance (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jobject list = NULL;
  GList *ret = NULL;

  gst_amc_jni_call_object_method (env, &err, self->object,
      android_hardware_camera_parameters.getSupportedWhiteBalance, &list);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getSupportedWhiteBalance: %s",
        err->message);
    g_clear_error (&err);
    return NULL;
  }

  if (list) {
    jobject iterator = NULL;

    iterator = java_util_list_iterator (env, list);
    if (iterator) {
      while (java_util_iterator_has_next (env, iterator)) {
        jobject str = java_util_iterator_next (env, iterator);

        if (str) {
          const gchar *value = _white_balance_to_gchar (env, str);

          ret = g_list_append (ret, (gchar *) value);
          gst_amc_jni_object_local_unref (env, str);
        }
      }
      gst_amc_jni_object_local_unref (env, iterator);
    }
    gst_amc_jni_object_local_unref (env, list);
  }

  return ret;
}

void
gst_ahc_parameters_supported_white_balance_free (GList * list)
{
  g_list_free (list);
}

gfloat
gst_ahc_parameters_get_vertical_view_angle (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  gfloat angle;

  gst_amc_jni_call_float_method (env, &err, self->object,
      android_hardware_camera_parameters.getVerticalViewAngle, &angle);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getVerticalViewAngle: %s",
        err->message);
    g_clear_error (&err);
    return 0.0;
  }

  return angle;
}

gboolean
gst_ahc_parameters_get_video_stabilization (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  gboolean ret;

  gst_amc_jni_call_boolean_method (env, &err, self->object,
      android_hardware_camera_parameters.getVideoStabilization, &ret);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getVideoStabilization: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return ret;
}

const gchar *
gst_ahc_parameters_get_white_balance (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  const gchar *ret = NULL;
  jstring white_balance;

  gst_amc_jni_call_object_method (env, &err, self->object,
      android_hardware_camera_parameters.getWhiteBalance, &white_balance);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getWhiteBalance: %s",
        err->message);
    g_clear_error (&err);
    return NULL;
  }

  ret = _white_balance_to_gchar (env, white_balance);

  if (white_balance)
    gst_amc_jni_object_local_unref (env, white_balance);

  return ret;
}

gint
gst_ahc_parameters_get_zoom (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  gint zoom;

  gst_amc_jni_call_int_method (env, &err, self->object,
      android_hardware_camera_parameters.getZoom, &zoom);
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.Parameters.getZoom: %s",
        err->message);
    g_clear_error (&err);
    return -1;
  }

  return zoom;
}

GList *
gst_ahc_parameters_get_zoom_ratios (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jobject list = NULL;
  GList *ret = NULL;

  gst_amc_jni_call_object_method (env, &err, self->object,
      android_hardware_camera_parameters.getZoomRatios, &list);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.getZoomRatios: %s",
        err->message);
    g_clear_error (&err);
    return NULL;
  }

  if (list) {
    jobject iterator = NULL;

    iterator = java_util_list_iterator (env, list);
    if (iterator) {
      while (java_util_iterator_has_next (env, iterator)) {
        jobject integer = java_util_iterator_next (env, iterator);

        if (integer) {
          jint value = java_lang_integer_int_value (env, integer);

          ret = g_list_append (ret, GINT_TO_POINTER (value));
          gst_amc_jni_object_local_unref (env, integer);
        }
      }
      gst_amc_jni_object_local_unref (env, iterator);
    }
    gst_amc_jni_object_local_unref (env, list);
  }

  return ret;
}

void
gst_ahc_parameters_zoom_ratios_free (GList * list)
{
  g_list_free (list);
}

gboolean
gst_ahc_parameters_is_smooth_zoom_supported (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  gboolean supported;

  gst_amc_jni_call_boolean_method (env, &err, self->object,
      android_hardware_camera_parameters.isSmoothZoomSupported, &supported);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.isSmoothZoomSupported: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return supported;
}

gboolean
gst_ahc_parameters_is_video_stabilization_supported (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  gboolean supported;

  gst_amc_jni_call_boolean_method (env, &err, self->object,
      android_hardware_camera_parameters.isVideoStabilizationSupported,
      &supported);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.isVideoStabilizationSupported: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return supported;
}

gboolean
gst_ahc_parameters_is_zoom_supported (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  gboolean supported;

  gst_amc_jni_call_boolean_method (env, &err, self->object,
      android_hardware_camera_parameters.isZoomSupported, &supported);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.isZoomSupported: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return supported;
}

gboolean
gst_ahc_parameters_set_antibanding (GstAHCParameters * self,
    const gchar * value)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jstring antibanding;

  antibanding = _antibanding_to_jstring (value);
  if (!antibanding)
    return FALSE;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera_parameters.setAntibanding, antibanding);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.setAntibanding: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_ahc_parameters_set_color_effect (GstAHCParameters * self,
    const gchar * value)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jstring color_effect;

  color_effect = _color_effect_to_jstring (value);
  if (!color_effect)
    return FALSE;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera_parameters.setColorEffect, color_effect);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.setColorEffect: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_ahc_parameters_set_exposure_compensation (GstAHCParameters * self,
    gint value)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera_parameters.setExposureCompensation, value);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.setExposureCompensation: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_ahc_parameters_set_flash_mode (GstAHCParameters * self, const gchar * value)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jstring flash_mode;

  flash_mode = _flash_mode_to_jstring (value);
  if (!flash_mode)
    return FALSE;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera_parameters.setFlashMode, flash_mode);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.setFlashMode: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_ahc_parameters_set_focus_mode (GstAHCParameters * self, const gchar * value)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jstring focus_mode;

  focus_mode = _focus_mode_to_jstring (value);
  if (!focus_mode)
    return FALSE;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera_parameters.setFocusMode, focus_mode);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.setFocusMode: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_ahc_parameters_set_preview_format (GstAHCParameters * self, gint format)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera_parameters.setPreviewFormat, format);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.setPreviewFormat: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_ahc_parameters_set_preview_fps_range (GstAHCParameters * self,
    gint min, gint max)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera_parameters.setPreviewFpsRange, min, max);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.setPreviewFpsRange: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_ahc_parameters_set_preview_size (GstAHCParameters * self,
    gint width, gint height)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera_parameters.setPreviewSize, width, height);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.setPreviewSize: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_ahc_parameters_set_scene_mode (GstAHCParameters * self, const gchar * value)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jstring scene_mode;

  scene_mode = _scene_mode_to_jstring (value);
  if (!scene_mode)
    return FALSE;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera_parameters.setSceneMode, scene_mode);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.setSceneMode: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}


gboolean
gst_ahc_parameters_set_video_stabilization (GstAHCParameters * self,
    gboolean toggle)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera_parameters.setVideoStabilization, toggle);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.setVideoStabilization: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_ahc_parameters_set_white_balance (GstAHCParameters * self,
    const gchar * value)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jstring white_balance;

  white_balance = _white_balance_to_jstring (value);
  if (!white_balance)
    return FALSE;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera_parameters.setWhiteBalance, white_balance);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.setWhiteBalance: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_ahc_parameters_set_zoom (GstAHCParameters * self, gint value)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera_parameters.setZoom, value);
  if (err) {
    GST_ERROR ("Failed to call android.hardware.Camera.Parameters.setZoom: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_ahc_parameters_unflatten (GstAHCParameters * self, const gchar * flattened)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jstring v_str = NULL;
  gboolean ret = TRUE;

  v_str = (*env)->NewStringUTF (env, flattened);
  if (v_str == NULL)
    return FALSE;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_camera_parameters.unflatten, v_str);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.Camera.Parameters.unflatten: %s",
        err->message);
    g_clear_error (&err);
    ret = FALSE;
  }

  gst_amc_jni_object_local_unref (env, self->object);

  return ret;
}

void
gst_ahc_parameters_free (GstAHCParameters * self)
{
  JNIEnv *env = gst_amc_jni_get_env ();

  g_return_if_fail (self != NULL);
  g_return_if_fail (self->object != NULL);

  GST_DEBUG ("freeing parameters %p", self->object);

  gst_amc_jni_object_unref (env, self->object);
  g_slice_free (GstAHCParameters, self);
}
