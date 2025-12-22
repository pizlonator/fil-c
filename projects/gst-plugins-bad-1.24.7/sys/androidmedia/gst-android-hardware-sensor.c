/*
 * Copyright (C) 2016 SurroundIO
 *   Author: Martin Kelly <martin@surround.io>
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
 *
 * The UNION_CAST macro is copyright:
 * Copyright (C) 2008-2016 Matt Gallagher ( http://cocoawithlove.com ).
 * All rights reserved.
 * Permission to use, copy, modify, and/or distribute this software for any purpose
 * with or without fee is hereby granted, provided that the above copyright notice
 * and this permission notice appear in all copies.

 * THE SOFTWARE IS PROVIDED “AS IS” AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <gmodule.h>

#include "gstjniutils.h"
#include "gst-android-hardware-sensor.h"

static jobject (*gst_android_get_application_context) (void) = NULL;

GST_DEBUG_CATEGORY_STATIC (ahs_debug);
#define GST_CAT_DEFAULT ahs_debug

/*
 * See:
 * http://www.cocoawithlove.com/2008/04/using-pointers-to-recast-in-c-is-bad.html
 * for details.
 */
#define UNION_CAST(x, destType) \
	(((union {__typeof__(x) a; destType b;})x).b)

static struct
{
  jclass klass;
  jstring SENSOR_SERVICE;
  jmethodID getSystemService;
} android_content_context = {
  0
};

static struct
{
  jclass klass;
  jfieldID accuracy;
  jfieldID values;
} android_hardware_sensor_event = {
  0
};

static struct
{
  jclass klass;
  jmethodID getDefaultSensor;;
  jmethodID registerListener;
  jmethodID unregisterListener;
} android_hardware_sensor_manager = {
  0
};

static struct
{
  jclass klass;
  jmethodID constructor;
} org_freedesktop_gstreamer_androidmedia_gstahscallback = {
  0
};

GHashTable *sensor_sizes = NULL;
static void
gst_ah_sensor_sensor_sizes_init (void)
{
  gint i;
  static struct
  {
    gint type;
    gsize size;
  } types[] = {
    {
        AHS_SENSOR_TYPE_ACCELEROMETER, sizeof (GstAHSAccelerometerValues)}
    , {
          AHS_SENSOR_TYPE_AMBIENT_TEMPERATURE,
        sizeof (GstAHSAmbientTemperatureValues)}
    , {
          AHS_SENSOR_TYPE_GAME_ROTATION_VECTOR,
        sizeof (GstAHSGameRotationVectorValues)}
    , {
          AHS_SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR,
        sizeof (GstAHSGeomagneticRotationVectorValues)}
    , {
        AHS_SENSOR_TYPE_GRAVITY, sizeof (GstAHSGravityValues)}
    , {
        AHS_SENSOR_TYPE_GYROSCOPE, sizeof (GstAHSGyroscopeValues)}
    , {
          AHS_SENSOR_TYPE_GYROSCOPE_UNCALIBRATED,
        sizeof (GstAHSGyroscopeUncalibratedValues)}
    , {
        AHS_SENSOR_TYPE_HEART_RATE, sizeof (GstAHSHeartRateValues)}
    , {
        AHS_SENSOR_TYPE_LIGHT, sizeof (GstAHSLightValues)}
    , {
          AHS_SENSOR_TYPE_LINEAR_ACCELERATION,
        sizeof (GstAHSLinearAccelerationValues)}
    , {
        AHS_SENSOR_TYPE_MAGNETIC_FIELD, sizeof (GstAHSMagneticFieldValues)}
    , {
          AHS_SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED,
        sizeof (GstAHSMagneticFieldUncalibratedValues)}
    , {
        AHS_SENSOR_TYPE_ORIENTATION, sizeof (GstAHSOrientationValues)}
    , {
        AHS_SENSOR_TYPE_PRESSURE, sizeof (GstAHSPressureValues)}
    , {
        AHS_SENSOR_TYPE_PROXIMITY, sizeof (GstAHSProximityValues)}
    , {
          AHS_SENSOR_TYPE_RELATIVE_HUMIDITY,
        sizeof (GstAHSRelativeHumidityValues)}
    , {
        AHS_SENSOR_TYPE_ROTATION_VECTOR, sizeof (GstAHSRotationVectorValues)}
    , {
        AHS_SENSOR_TYPE_STEP_COUNTER, sizeof (GstAHSStepCounterValues)}
    , {
        AHS_SENSOR_TYPE_STEP_DETECTOR, sizeof (GstAHSStepDetectorValues)}
    ,
  };

  g_assert_null (sensor_sizes);

  sensor_sizes = g_hash_table_new (g_int_hash, g_int_equal);
  for (i = 0; i < G_N_ELEMENTS (types); i++)
    g_hash_table_insert (sensor_sizes, &types[i].type, &types[i].size);
}

static void
gst_ah_sensor_sensor_sizes_deinit (void)
{
  g_hash_table_unref (sensor_sizes);
  sensor_sizes = NULL;
}

gsize
gst_ah_sensor_get_sensor_data_size (gint sensor_type)
{
  return *((gsize *) g_hash_table_lookup (sensor_sizes, &sensor_type));
}

static void
gst_ah_sensor_on_sensor_changed (JNIEnv * env, jclass klass,
    jobject sensor_event, jlong callback, jlong user_data)
{
  GstAHSensorCallback cb = (GstAHSensorCallback) (gsize) callback;

  if (cb)
    cb (sensor_event, (gpointer) (gsize) user_data);
}

static void
gst_ah_sensor_on_accuracy_changed (JNIEnv * env, jclass klass,
    jobject sensor, jint accuracy, jlong callback, jlong user_data)
{
  GstAHSAccuracyCallback cb = (GstAHSAccuracyCallback) (gsize) callback;

  if (cb)
    cb (sensor, accuracy, (gpointer) (gsize) user_data);
}

static gboolean natives_registered = FALSE;

static JNINativeMethod native_methods[] = {
  {(gchar *) "gst_ah_sensor_on_sensor_changed",
        (gchar *) "(Landroid/hardware/SensorEvent;JJ)V",
      (void *) gst_ah_sensor_on_sensor_changed},
  {(gchar *) "gst_ah_sensor_on_accuracy_changed",
        (gchar *) "(Landroid/hardware/Sensor;IJJ)V",
      (void *) gst_ah_sensor_on_accuracy_changed}
};

static gboolean
_init_classes (void)
{
  gint32 delay;
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jclass klass;
  jfieldID fieldID;
  GModule *module;
  gboolean success;
  gint32 type;

  /*
   * Lookup the Android function to get an Android context. This function will
   * be provided when the plugin is built via ndk-build.
   */
  module = g_module_open (NULL, G_MODULE_BIND_LOCAL);
  if (!module)
    goto failed;
  success = g_module_symbol (module, "gst_android_get_application_context",
      (gpointer *) & gst_android_get_application_context);
  if (!success || !gst_android_get_application_context)
    goto failed;
  g_module_close (module);

  /* android.content.Context */
  klass = android_content_context.klass = gst_amc_jni_get_class (env, &err,
      "android/content/Context");
  if (!klass)
    goto failed;
  android_content_context.getSystemService =
      gst_amc_jni_get_method_id (env, &err, klass, "getSystemService",
      "(Ljava/lang/String;)Ljava/lang/Object;");
  if (!android_content_context.getSystemService)
    goto failed;

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SENSOR_SERVICE",
      "Ljava/lang/String;");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_object_field (env, &err, klass, fieldID,
          &android_content_context.SENSOR_SERVICE))
    goto failed;
  android_content_context.SENSOR_SERVICE =
      gst_amc_jni_object_make_global (env,
      android_content_context.SENSOR_SERVICE);
  if (!android_content_context.SENSOR_SERVICE)
    goto failed;

  /* android.hardware.SensorEvent */
  klass = android_hardware_sensor_event.klass =
      gst_amc_jni_get_class (env, &err, "android/hardware/SensorEvent");
  if (!klass)
    goto failed;
  android_hardware_sensor_event.accuracy =
      gst_amc_jni_get_field_id (env, &err, klass, "accuracy", "I");
  if (!android_hardware_sensor_event.accuracy)
    goto failed;
  android_hardware_sensor_event.values =
      gst_amc_jni_get_field_id (env, &err, klass, "values", "[F");
  if (!android_hardware_sensor_event.values)
    goto failed;

  /* android.hardware.SensorManager */
  klass = android_hardware_sensor_manager.klass =
      gst_amc_jni_get_class (env, &err, "android/hardware/SensorManager");
  if (!klass)
    goto failed;
  android_hardware_sensor_manager.getDefaultSensor =
      gst_amc_jni_get_method_id (env, &err, klass,
      "getDefaultSensor", "(I)Landroid/hardware/Sensor;");
  if (!android_hardware_sensor_manager.getDefaultSensor)
    goto failed;
  android_hardware_sensor_manager.registerListener =
      gst_amc_jni_get_method_id (env, &err, klass,
      "registerListener",
      "(Landroid/hardware/SensorEventListener;Landroid/hardware/Sensor;I)Z");
  if (!android_hardware_sensor_manager.registerListener)
    goto failed;
  android_hardware_sensor_manager.unregisterListener =
      gst_amc_jni_get_method_id (env, &err, klass,
      "unregisterListener", "(Landroid/hardware/SensorEventListener;)V");
  if (!android_hardware_sensor_manager.unregisterListener)
    goto failed;

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SENSOR_DELAY_FASTEST",
      "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &delay))
    goto failed;
  if (delay != AHS_SENSOR_DELAY_FASTEST) {
    GST_ERROR ("SENSOR_DELAY_FASTEST has changed value");
    goto failed;
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SENSOR_DELAY_GAME",
      "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &delay))
    goto failed;
  if (delay != AHS_SENSOR_DELAY_GAME) {
    GST_ERROR ("SENSOR_DELAY_GAME has changed value");
    goto failed;
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SENSOR_DELAY_NORMAL",
      "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &delay))
    goto failed;
  if (delay != AHS_SENSOR_DELAY_NORMAL) {
    GST_ERROR ("SENSOR_DELAY_NORMAL has changed value");
    goto failed;
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "SENSOR_DELAY_UI",
      "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &delay))
    goto failed;
  if (delay != AHS_SENSOR_DELAY_UI) {
    GST_ERROR ("SENSOR_DELAY_UI has changed value");
    goto failed;
  }

  /* android.hardware.Sensor */
  klass = gst_amc_jni_get_class (env, &err, "android/hardware/Sensor");
  if (!klass)
    goto failed;

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "TYPE_ACCELEROMETER",
      "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &type))
    goto failed;
  if (type != AHS_SENSOR_TYPE_ACCELEROMETER) {
    GST_ERROR ("TYPE_ACCELEROMETER has changed value");
    goto failed;
  }

  fieldID = gst_amc_jni_get_static_field_id (env, &err, klass,
      "TYPE_AMBIENT_TEMPERATURE", "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &type))
    goto failed;
  if (type != AHS_SENSOR_TYPE_AMBIENT_TEMPERATURE) {
    GST_ERROR ("TYPE_AMBIENT_TEMPERATURE has changed value");
    goto failed;
  }

  fieldID = gst_amc_jni_get_static_field_id (env, &err, klass,
      "TYPE_GAME_ROTATION_VECTOR", "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &type))
    goto failed;
  if (type != AHS_SENSOR_TYPE_GAME_ROTATION_VECTOR) {
    GST_ERROR ("TYPE_GAME_ROTATION_VECTOR has changed value");
    goto failed;
  }

  fieldID = gst_amc_jni_get_static_field_id (env, &err, klass,
      "TYPE_GEOMAGNETIC_ROTATION_VECTOR", "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &type))
    goto failed;
  if (type != AHS_SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR) {
    GST_ERROR ("TYPE_GEOMAGNETIC_ROTATION_VECTOR has changed value");
    goto failed;
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "TYPE_GRAVITY", "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &type))
    goto failed;
  if (type != AHS_SENSOR_TYPE_GRAVITY) {
    GST_ERROR ("TYPE_GRAVITY has changed value");
    goto failed;
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "TYPE_GYROSCOPE", "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &type))
    goto failed;
  if (type != AHS_SENSOR_TYPE_GYROSCOPE) {
    GST_ERROR ("TYPE_GYROSCOPE has changed value");
    goto failed;
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass,
      "TYPE_GYROSCOPE_UNCALIBRATED", "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &type))
    goto failed;
  if (type != AHS_SENSOR_TYPE_GYROSCOPE_UNCALIBRATED) {
    GST_ERROR ("TYPE_GYROSCOPE_UNCALIBRATED has changed value");
    goto failed;
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "TYPE_HEART_RATE",
      "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &type))
    goto failed;
  if (type != AHS_SENSOR_TYPE_HEART_RATE) {
    GST_ERROR ("TYPE_HEART_RATE has changed value");
    goto failed;
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "TYPE_LIGHT", "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &type))
    goto failed;
  if (type != AHS_SENSOR_TYPE_LIGHT) {
    GST_ERROR ("TYPE_LIGHT has changed value");
    goto failed;
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass,
      "TYPE_LINEAR_ACCELERATION", "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &type))
    goto failed;
  if (type != AHS_SENSOR_TYPE_LINEAR_ACCELERATION) {
    GST_ERROR ("TYPE_LINEAR_ACCELERATION has changed value");
    goto failed;
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "TYPE_MAGNETIC_FIELD",
      "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &type))
    goto failed;
  if (type != AHS_SENSOR_TYPE_MAGNETIC_FIELD) {
    GST_ERROR ("TYPE_MAGNETIC_FIELD has changed value");
    goto failed;
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass,
      "TYPE_MAGNETIC_FIELD_UNCALIBRATED", "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &type))
    goto failed;
  if (type != AHS_SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED) {
    GST_ERROR ("TYPE_MAGNETIC_FIELD_UNCALIBRATED has changed value");
    goto failed;
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "TYPE_ORIENTATION",
      "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &type))
    goto failed;
  if (type != AHS_SENSOR_TYPE_ORIENTATION) {
    GST_ERROR ("TYPE_ORIENTATION has changed value");
    goto failed;
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "TYPE_PRESSURE", "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &type))
    goto failed;
  if (type != AHS_SENSOR_TYPE_PRESSURE) {
    GST_ERROR ("TYPE_PRESSURE has changed value");
    goto failed;
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "TYPE_PROXIMITY", "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &type))
    goto failed;
  if (type != AHS_SENSOR_TYPE_PROXIMITY) {
    GST_ERROR ("TYPE_PROXIMITY has changed value");
    goto failed;
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass,
      "TYPE_RELATIVE_HUMIDITY", "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &type))
    goto failed;
  if (type != AHS_SENSOR_TYPE_RELATIVE_HUMIDITY) {
    GST_ERROR ("TYPE_RELATIVE_HUMIDITY has changed value");
    goto failed;
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "TYPE_ROTATION_VECTOR",
      "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &type))
    goto failed;
  if (type != AHS_SENSOR_TYPE_ROTATION_VECTOR) {
    GST_ERROR ("TYPE_ROTATION_VECTOR has changed value");
    goto failed;
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass,
      "TYPE_SIGNIFICANT_MOTION", "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &type))
    goto failed;
  if (type != AHS_SENSOR_TYPE_SIGNIFICANT_MOTION) {
    GST_ERROR ("TYPE_SIGNIFICANT_MOTION has changed value");
    goto failed;
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "TYPE_STEP_COUNTER",
      "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &type))
    goto failed;
  if (type != AHS_SENSOR_TYPE_STEP_COUNTER) {
    GST_ERROR ("TYPE_STEP_COUNTER has changed value");
    goto failed;
  }

  fieldID =
      gst_amc_jni_get_static_field_id (env, &err, klass, "TYPE_STEP_DETECTOR",
      "I");
  if (!fieldID)
    goto failed;
  if (!gst_amc_jni_get_static_int_field (env, &err, klass, fieldID, &type))
    goto failed;
  if (type != AHS_SENSOR_TYPE_STEP_DETECTOR) {
    GST_ERROR ("TYPE_STEP_DETECTOR has changed value");
    goto failed;
  }

  /* org.freedesktop.gstreamer.androidmedia.GstAhsCallback */
  if (!org_freedesktop_gstreamer_androidmedia_gstahscallback.klass) {
    org_freedesktop_gstreamer_androidmedia_gstahscallback.klass =
        gst_amc_jni_get_application_class (env,
        "org/freedesktop/gstreamer/androidmedia/GstAhsCallback", &err);
  }
  if (!org_freedesktop_gstreamer_androidmedia_gstahscallback.klass)
    goto failed;
  org_freedesktop_gstreamer_androidmedia_gstahscallback.constructor =
      gst_amc_jni_get_method_id (env, &err,
      org_freedesktop_gstreamer_androidmedia_gstahscallback.klass, "<init>",
      "(JJJ)V");
  if (!org_freedesktop_gstreamer_androidmedia_gstahscallback.constructor)
    goto failed;

  if ((*env)->RegisterNatives (env,
          org_freedesktop_gstreamer_androidmedia_gstahscallback.klass,
          native_methods, G_N_ELEMENTS (native_methods))) {
    GST_ERROR ("Failed to register native methods for GstAhsCallback");
    goto failed;
  }
  natives_registered = TRUE;

  return TRUE;

failed:
  if (err) {
    GST_ERROR ("Failed to initialize Android classes: %s", err->message);
    g_clear_error (&err);
  }

  return FALSE;
}

gboolean
gst_android_hardware_sensor_init (void)
{
  GST_DEBUG_CATEGORY_INIT (ahs_debug, "ahs", 0,
      "Android Gstreamer Hardware Sensor");
  if (!_init_classes ()) {
    gst_android_hardware_sensor_deinit ();
    return FALSE;
  }

  gst_ah_sensor_sensor_sizes_init ();

  return TRUE;
}

void
gst_android_hardware_sensor_deinit (void)
{
  JNIEnv *env = gst_amc_jni_get_env ();

  if (android_content_context.SENSOR_SERVICE) {
    gst_amc_jni_object_unref (env, android_content_context.SENSOR_SERVICE);
    android_content_context.SENSOR_SERVICE = NULL;
  }

  if (android_content_context.klass) {
    gst_amc_jni_object_unref (env, android_content_context.klass);
    android_content_context.klass = NULL;
  }

  if (android_hardware_sensor_event.klass) {
    gst_amc_jni_object_unref (env, android_hardware_sensor_event.klass);
    android_hardware_sensor_event.klass = NULL;
  }

  if (android_hardware_sensor_manager.klass) {
    gst_amc_jni_object_unref (env, android_hardware_sensor_manager.klass);
    android_hardware_sensor_manager.klass = NULL;
  }

  if (org_freedesktop_gstreamer_androidmedia_gstahscallback.klass) {
    if (natives_registered) {
      (*env)->UnregisterNatives (env,
          org_freedesktop_gstreamer_androidmedia_gstahscallback.klass);
      natives_registered = FALSE;
    }
    gst_amc_jni_object_unref (env,
        org_freedesktop_gstreamer_androidmedia_gstahscallback.klass);
    org_freedesktop_gstreamer_androidmedia_gstahscallback.klass = NULL;
  }

  if (sensor_sizes)
    gst_ah_sensor_sensor_sizes_deinit ();
}

GstAHSensorManager *
gst_ah_sensor_get_manager (void)
{
  jobject context;
  GError *err = NULL;
  JNIEnv *env = gst_amc_jni_get_env ();
  GstAHSensorManager *manager;
  jobject object;
  gboolean success;

  context = gst_android_get_application_context ();
  success = gst_amc_jni_call_object_method (env, &err, context,
      android_content_context.getSystemService,
      &object, android_content_context.SENSOR_SERVICE);
  if (!success)
    return NULL;

  object = gst_amc_jni_object_make_global (env, object);
  if (!object)
    return NULL;

  manager = g_slice_new (GstAHSensorManager);
  manager->object = object;

  return manager;
}

GstAHSensor *
gst_ah_sensor_get_default_sensor (GstAHSensorManager * self, gint32 sensor_type)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jobject object;
  GstAHSensor *sensor;

  if (!gst_amc_jni_call_object_method (env, &err, self->object,
          android_hardware_sensor_manager.getDefaultSensor,
          &object, sensor_type))
    return NULL;

  object = gst_amc_jni_object_make_global (env, object);
  if (!object)
    return NULL;

  sensor = g_slice_new (GstAHSensor);
  sensor->object = object;

  return sensor;
}

GstAHSensorEventListener *
gst_ah_sensor_create_listener (GstAHSensorCallback sensor_cb,
    GstAHSAccuracyCallback accuracy_cb, gpointer user_data)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  GstAHSensorEventListener *listener;
  jobject object;

  object = gst_amc_jni_new_object (env,
      &err,
      TRUE,
      org_freedesktop_gstreamer_androidmedia_gstahscallback.klass,
      org_freedesktop_gstreamer_androidmedia_gstahscallback.constructor,
      UNION_CAST (sensor_cb, jlong),
      UNION_CAST (accuracy_cb, jlong), UNION_CAST (user_data, jlong));
  if (err) {
    GST_ERROR ("Failed to create listener callback class");
    g_clear_error (&err);
    return NULL;
  }

  listener = g_slice_new (GstAHSensorEventListener);
  listener->object = object;

  return listener;
}

gboolean
gst_ah_sensor_register_listener (GstAHSensorManager * self,
    GstAHSensorEventListener * listener, GstAHSensor * sensor, gint32 delay)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  gboolean success;

  gst_amc_jni_call_boolean_method (env, &err, self->object,
      android_hardware_sensor_manager.registerListener, &success,
      listener->object, sensor->object, (jint) delay);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.SensorManager.registerListener: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }
  listener->registered = TRUE;

  return TRUE;
}

void
gst_ah_sensor_unregister_listener (GstAHSensorManager * self,
    GstAHSensorEventListener * listener)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;

  gst_amc_jni_call_void_method (env, &err, self->object,
      android_hardware_sensor_manager.unregisterListener, listener->object);
  if (err) {
    GST_ERROR
        ("Failed to call android.hardware.SensorManager.unregisterListener: %s",
        err->message);
    g_clear_error (&err);
  }
  listener->registered = FALSE;
}

gboolean
gst_ah_sensor_populate_event (GstAHSensorEvent * event, jobject event_object,
    gint size)
{
  JNIEnv *env = gst_amc_jni_get_env ();
  GError *err = NULL;
  jfloatArray object_array;
  jfloat *values;

  gst_amc_jni_get_int_field (env, &err,
      event_object, android_hardware_sensor_event.accuracy, &event->accuracy);
  if (err) {
    GST_ERROR ("Failed to get sensor accuracy field: %s", err->message);
    goto error;
  }

  gst_amc_jni_get_object_field (env, &err, event_object,
      android_hardware_sensor_event.values, &object_array);
  if (err) {
    GST_ERROR ("Failed to get sensor values field: %s", err->message);
    goto error;
  }

  values = (*env)->GetFloatArrayElements (env, object_array, NULL);
  if (!values) {
    GST_ERROR ("Failed to get float array elements from object array");
    gst_amc_jni_object_local_unref (env, object_array);
    return FALSE;
  }
  /* We can't use gst_amc_jni_object_make_global here because we need to call
   * ReleaseFloatArrayElements before doing a local unref in the failure case,
   * but gst_amc_jni_object_make_global would unref before we could Release.
   */
  event->data.array = gst_amc_jni_object_ref (env, object_array);
  if (!event->data.array) {
    (*env)->ReleaseFloatArrayElements (env, object_array, values, JNI_ABORT);
    gst_amc_jni_object_local_unref (env, object_array);
    return FALSE;
  }
  event->data.values = values;
  gst_amc_jni_object_local_unref (env, object_array);

  return TRUE;

error:
  g_clear_error (&err);
  return FALSE;
}

void
gst_ah_sensor_free_sensor_data (GstAHSensorData * data)
{
  JNIEnv *env = gst_amc_jni_get_env ();

  (*env)->ReleaseFloatArrayElements (env, data->array, data->values, JNI_ABORT);
  gst_amc_jni_object_unref (env, data->array);
}
